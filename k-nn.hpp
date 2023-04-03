#ifndef k_nn_HPP_
#define k_nn_HPP_

#include <iostream>
#include <cassert>
#include <memory>
#include <numeric>
#include <algorithm>
#include <random>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cmath>
#include <sstream>
#include <fstream>

using namespace std;

using coors_t = vector<vector<float>>;
using vector_int = vector<int>;

const int MIN_SAMPLE_SIZE = 80;
const int BATCH_SIZE = 7;


struct Node {
  Node() :
    level_{0}, left_{nullptr}, right_{nullptr} {}

  Node(int l) :
    level_{l}, left_{nullptr}, right_{nullptr} {}

  int level_;
  int index_;
  unique_ptr<Node> left_;
  unique_ptr<Node> right_;
};


struct Job {
  Node *node;
  vector_int indices;

  Job(Node *n, vector_int i) :
    node{n}, indices{i} {};
};


class Tree {
  public:
    Tree(coors_t &p, int d, int num_threads);
    void query(coors_t *qs, int d, int k, int num_threads);
    void write(string result, uint64_t tid, uint64_t qid, uint64_t nqs, uint64_t k);

  private:
    unique_ptr<Node> root_;
    coors_t &coor_;
    coors_t *queries_;
    int dims_;
    condition_variable job_cv_;
    queue<Job> job_q_;
    mutex job_mtx_;
    atomic<size_t> num_nodes_;
    atomic<size_t> next_batch_;
    coors_t results_;

    int sample_median_index(const vector_int &indices, const int dim);
    pair<vector_int, vector_int> split(int pivot_i, vector_int &indices, int dim);
    void grow(int tid);
    vector<float> node_val(Node *node);
    void process_query_batch(int tid, int k);
    float distance(vector<float> a, vector<float> b);
    Node *closer_node(vector<float> target, Node *a, Node *b);
    Node *process_query(Node *node, size_t qi, int depth, int k, vector<Node*> &heap);
};


Tree::Tree(coors_t &p, int d, int num_threads) :
    coor_(p), dims_(d), num_nodes_(1) {
  vector_int indices(coor_.size());
  iota(indices.begin(), indices.end(), 0);
  int median = sample_median_index(indices, 0);
  vector_int left, right;
  tie(left, right) = split(median, indices, 0);
  root_ = make_unique<Node>(0);
  root_->index_ = median;
  if (!left.empty()) {
    root_->left_ = make_unique<Node>(1);
    ++num_nodes_;
    job_q_.emplace(root_->left_.get(), left);
  }

  if (!right.empty()) {
    root_->right_ = make_unique<Node>(1);
    ++num_nodes_;
    job_q_.emplace(root_->right_.get(), right);
  }

  vector<thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.push_back(thread(&Tree::grow, this, i));
  }
  for (int i = 0; i < num_threads; ++i) {
    threads[i].join();
  }
}


void Tree::query(coors_t *qs, int d, int k, int num_threads) {
  assert(d == dims_);
  queries_ = qs;
  next_batch_ = 0;
  results_.resize(queries_->size() * k);
  vector<thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.push_back(thread(&Tree::process_query_batch, this, i, k));
  }
  for (int i = 0; i < num_threads; ++i) {
    threads[i].join();
  }

}

void Tree::write(string result, uint64_t tid, uint64_t qid, uint64_t nqs, uint64_t k) {

  ofstream file;
  file.open(result, ios_base::binary);
  assert(file.is_open());
  file.write("RESULT", 8);
  file.write((char*) &tid, 8);
  file.write((char*) &qid, 8);
  file.write((char*) &qid + 1, 8);
  file.write((char*) &nqs, 8);
  file.write((char*) &dims_, 8);
  file.write((char*) &k, 8);
  for (auto result : results_) {
    for (auto val : result) {
      file.write((char*) &val, sizeof(float));
    }
  }
  file.close();
}

int Tree::sample_median_index(const vector_int &indices, const int dim) {
  vector_int s;
  sample(indices.begin(), indices.end(),
      back_inserter(s), min(MIN_SAMPLE_SIZE, (int)indices.size()),
      mt19937{random_device{}()});

  sort(s.begin(), s.end(), [&](int a, int b) {
    return coor_[a][dim] < coor_[b][dim];
  });

  return s[s.size() / 2];
}


pair<vector_int, vector_int> Tree::split(int pivot_i, vector_int &indices, int dim) {
  vector_int left;
  vector_int right;
  float pivot = coor_[pivot_i][dim];

  for (auto i : indices) {
    if (i != pivot_i) {
      float cur = coor_[i][dim];
      if (cur < pivot) left.push_back(i);
      else right.push_back(i);
    }
  }
  assert(
      (indices.size() == left.size() + right.size() + 1) ||
      !(cerr << left.size() << " + " << right.size() << " + 1 = " <<
        (left.size() + right.size() + 1) << " =/= "
        << indices.size() << endl));
  return pair<vector_int, vector_int>(left, right);
}


void Tree::grow(int tid) {
  assert(tid >= 0);
  for (;;) {
    unique_lock<mutex> job_q_lock(job_mtx_);
    while (job_q_.empty()) {
      if (num_nodes_ == coor_.size()) return;
      job_cv_.wait(job_q_lock);
    }
    Job j = move(job_q_.front());
    job_q_.pop();
    job_q_lock.unlock();
    job_cv_.notify_all();
    assert(!j.indices.empty());
    if (j.indices.size() == 1) {
      j.node->index_ = j.indices[0];
      continue;
    }

    int median = sample_median_index(j.indices, j.node->level_ % dims_);
    vector_int left, right;
    tie(left, right) = split(median, j.indices, 0);
    j.node->index_ = median;
    if (!left.empty()) {
      j.node->left_ = make_unique<Node>(j.node->level_ + 1);
      num_nodes_++;
    }
    if (!right.empty()) {
      j.node->right_ = make_unique<Node>(j.node->level_ + 1);
      num_nodes_++;
    }
    job_q_lock.lock();
    if (!left.empty()) {
      job_q_.emplace(j.node->left_.get(), left);
    }
    if (!right.empty()) {
      job_q_.emplace(j.node->right_.get(), right);
    }
    job_q_lock.unlock();
  }
}


vector<float> Tree::node_val(Node *node) {
  return coor_[node->index_];
}


void Tree::process_query_batch(int tid, int k) {
  assert(tid >= 0);
  for (;;) {
    size_t batch_start = next_batch_;
    do {
      if (batch_start >= queries_->size()) return;
    } while (!atomic_compare_exchange_weak(&next_batch_, &batch_start, batch_start + BATCH_SIZE));
    this_thread::sleep_for(chrono::milliseconds(200));
    auto batch_end = min(batch_start + BATCH_SIZE, queries_->size());
    for (auto i = batch_start; i < batch_end; i++) {
      vector<Node*> heap;
      process_query(root_.get(), i, 0, k, heap);
      for (size_t j = 0; j < heap.size(); j++) {
        results_[i * k + j] = node_val(heap[j]);
      }
    }
  }
}


float Tree::distance(vector<float> a, vector<float> b) {
  assert(a.size() == b.size());
  float sum = 0;
  for (size_t i = 0; i < a.size(); i++) {
    float dif = a[i] - b[i];
    sum += dif * dif;
  }
  return sqrt(sum);
}

Node *Tree::closer_node(vector<float> target, Node *a, Node *b) {
  if (!a) return b;
  else if (!b) return a;
  auto da = distance(target, node_val(a));
  auto db = distance(target, node_val(b));
  if (da < db)  return a;
  return b;
}


Node *Tree::process_query(Node *node, size_t qi, int depth, int k, vector<Node*> &heap) {
  if (!node) return nullptr;
  vector<float> target = (*queries_)[qi];
  auto comp = [&](Node *a, Node *b){
    if (a == closer_node(target, a, b)) return true;
    return false;
  };

  if (!node->left_ && !node->right_) {
    if (node == closer_node(target, node, heap[0])) {
      heap[0] = node;
    }
    make_heap(heap.begin(), heap.end(), comp);
    return node;
  }
  int dim = depth % dims_;
  Node *next_branch = nullptr;
  Node *oppo_branch = nullptr;

  ostringstream os;
  os << "query = " << qi
     << "\tdepth = " << depth
     << "\tdim = " << dim
     << "\ttarget[dim] = " << target[dim]
     << "\tnode[dim] = " << node_val(node)[dim];
  if (target[dim] < node_val(node)[dim]) {
    next_branch = node->left_.get();
    oppo_branch = node->right_.get();
    os << "\tNext branch is left\n";
  } else {
    next_branch = node->right_.get();
    oppo_branch = node->left_.get();
    os << "\tNext branch is right\n";
  }
  if (heap.size() < (unsigned)k) heap.push_back(node);
  else if (heap.size() == (unsigned)k && node == closer_node(target, node, heap[0]) ) heap[0] = node;
  make_heap(heap.begin(), heap.end(), comp);
  Node *best = closer_node(target, node, process_query(next_branch, qi, depth + 1, k, heap));
  auto dist_to_best = distance(target, node_val(best));
  auto dist_to_split = abs(target[dim] - node_val(node)[dim]);

  if (dist_to_best > dist_to_split) best = closer_node(target, best, process_query(oppo_branch, qi, depth + 1, k, heap));
  return best;
}


#endif
