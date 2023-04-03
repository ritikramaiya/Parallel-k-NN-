#ifndef _DATA_HPP_
#define _DATA_HPP_

#include <iostream>
#include <cstdlib>
#include <string>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include <string.h>
#include <vector>

using namespace std;
using coors_t = vector<vector<float>>;

class Reader {
    public:
        Reader(const char *p) : ptr{p} {}
        template <typename T>
        Reader &operator>>(T &o) {
            // Assert alignment.
            assert(uintptr_t(ptr)%sizeof(T) == 0);
            o = *(T *) ptr;
            ptr += sizeof(T);
            return *this;
        }
    private:
        const char *ptr;
};

void check_file(const string fn, int fd) {
  if (fd < 0) {
    int en = errno;
    cerr << "Couldn't open " << fn << ": " <<
      strerror(en) << "." << endl;
    exit(2);
  }
}


off_t get_size(int fd) {
  struct stat sb;
  int rv = fstat(fd, &sb);
  assert(rv == 0);
  return sb.st_size;
}


void *mmap_wrapper(off_t file_size, int fd) {
  void *vp = mmap(nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0);
  if (vp == MAP_FAILED) {
    int en = errno;
    fprintf(stderr, "mmap() failed: %s\n", strerror(en));
    exit(3);
  }
  return vp;
}


coors_t parse_coors(Reader &reader, uint64_t n_coors, uint64_t n_dims) {
  coors_t coors;
  for (uint64_t i = 0; i < n_coors; i++) {
    vector<float> coor;
    for (uint64_t j = 0; j < n_dims; j++) {
      float f;
      reader >> f;
      coor.push_back(f);
    }
    coors.push_back(coor);
  }
  return coors;
}


void print_coors(coors_t coors) {
  for (auto p : coors) {
    cout << "\tCoors: ";
    for (auto v : p) {
      cout << fixed << setprecision(6) << setw(15) << setfill(' ') << v;
    }
    cout << endl;
  }
}


struct Data {
  string file_type;
  uint64_t id;
  uint64_t n_coors;
  uint64_t n_dims;
  uint64_t k = 0;
  coors_t coors;
};


Data parse_data(const string &fn) {
  int fd = open(fn.c_str(), O_RDONLY);
  check_file(fn, fd);
  auto file_size = get_size(fd);
  void *vp = mmap_wrapper(file_size, fd);
  char *file_mem = (char *) vp;
  int rv = madvise(vp, file_size, MADV_SEQUENTIAL|MADV_WILLNEED); assert(rv == 0);
  rv = close(fd); assert(rv == 0);
  int n = strnlen(file_mem, 8);
  string file_type(file_mem, n);
  Reader reader{file_mem + 8};
  uint64_t id, n_coors, n_dims, k = 0;
  reader >> id >> n_coors >> n_dims;
  if (file_type == "QUERY") reader >> k;
  auto coors = parse_coors(reader, n_coors, n_dims);
  Data data = {file_type, id, n_coors, n_dims, k, coors};
  return data;
}


void print_data(Data d) {
  cout << "\tFile type string: " << d.file_type << endl;
  cout << "\tTraining file ID: " << hex << setw(16) << setfill('0') << d.id << dec << endl;
  cout << "\tNumber of points: " << d.n_coors << endl;
  cout << "\tNumber of dimensions: " << d.n_dims << endl;
  if (d.file_type == "QUERY") cout << "\tNumber of neighbors: " << d.k << endl;
  print_coors(d.coors);
  cout << endl;
}

#endif
