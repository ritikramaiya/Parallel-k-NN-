#include "k-nn.hpp"
#include "data.hpp"
#include <vector>
#include <chrono>

using namespace std;
int main(int argc, char* argv[]){
  if(argc < 5){
    cout << "./k-nn n_cores training_file query_file result_file\n";
    return 0;
  }
  int numCores = stoi(argv[1]);

  cout << "\ntotal time to build tree:\n";
  auto training = parse_data(argv[2]);
  auto start = chrono::high_resolution_clock::now();
  Tree tree(training.coors, training.n_dims, numCores);

  auto stop = chrono::high_resolution_clock::now();
  chrono::duration<double> dt = stop - start;
  auto ms = chrono::duration_cast<chrono::milliseconds>(dt);
  cout << "\nTime: " << ms.count() << " ms."<< endl;


  cout << "\ntime to execute Queries:" << endl;
  auto queries = parse_data(argv[3]);
  start = chrono::high_resolution_clock::now();
  tree.query(&(queries.coors), queries.n_dims, queries.k, numCores);
  stop = chrono::high_resolution_clock::now();
  dt = stop - start;
  ms = chrono::duration_cast<chrono::milliseconds>(dt);
  cout << "\nTime: " << ms.count() << " ms."<< endl;
  tree.write(argv[4], training.id, queries.id, queries.n_coors, queries.k);

  return 0;
}


// Ritik Ramaiya