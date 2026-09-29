#include "mmx_source_assets.h"
#include <cstdio>
#include <cstdlib>
int main(int argc,char **argv) {
  if (argc!=5) { std::fprintf(stderr,"Usage: extractor ROM GAME ZERO OUTPUT\n");return 2; }
  char error[512];
  if (!MmxSourceAssetsBuild(argv[1],unsigned(std::atoi(argv[2])),std::atoi(argv[3]),argv[4],error,sizeof(error))) {
    std::fprintf(stderr,"%s\n",error);return 1;
  }
  return 0;
}
