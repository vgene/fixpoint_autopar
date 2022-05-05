/*
 * A simple program to grep for a pattern in files in a directory recursively.
 *
 */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using std::string;
using std::vector;

// Find all file names in a directory recursively
vector<string> find_files(const string &dir) {
  std::vector<string> files;

  // go through all files in the directory
  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    // ignore symbolic links
    if (entry.is_symlink()) {
      continue;
    }
    // if it's a directory, recursively call find_files
    if (std::filesystem::is_directory(entry.path())) {
      vector<string> sub_files = find_files(entry.path().string());
      files.insert(files.end(), sub_files.begin(), sub_files.end());
    } else {
      // only take file names with .c extension
      files.push_back(entry.path().string());
    }
  }

  return files;
}

// Find all patterns in a file and print to the fstream
void grep(const string &file, const string &pattern, std::ostream &out) {
  std::ifstream in(file);
  string line;
  while (std::getline(in, line)) {
    if (line.find(pattern) != string::npos) {
      out << file << ": " << line << std::endl;
    }
  }
}

int main(int argc, char *argv[]) {
  // print the patterns for all files in the
  // directory recursively
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <pattern> <dir>" << std::endl;
    return 1;
  }

  auto files = find_files(argv[2]);

  string pattern = argv[1];

  // for each file, grep for the pattern
  for (const auto &file : files) {
    grep(file, pattern, std::cout);
  }

  return 0;
}
