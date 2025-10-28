/*
 * Made by Yousef Awad in C++ using CMake for the compilation.
 * Yay!
*/

/*
 * The following is all of the return types and what they signify:
 * 0: Program ran succesfully
 * 1: printf error in main
 * 
*/

#include <fstream>
#include <cstdio>
#include <iostream>

#define NUM_SETS 1
#define ASSOC 1
#define BLOCK_SIZE 64

long long int tag[NUM_SETS][ASSOC];
long long int lru_pos[NUM_SETS][ASSOC];
bool dirty[NUM_SETS][ASSOC];

// statistic variables
unsigned int hit  = 0;
unsigned int miss = 0;

void update_lru(long long int add)
{
}

void update_fifo(long long int add)
{
}

void simulate_access(char opcode, long long int add)
{
  int set = (add / BLOCK_SIZE) % NUM_SETS;
  long long int tag_accessing = add / BLOCK_SIZE;

  for (int i = 0; i < ASSOC; i++)
  {
    if (tag_accessing == tag[set][i])
    {
      // Cache hit!!
      hit++;
      if (/* LRU policy goes here */)
      {
        update_lru(add);
      }
      else
      {
        update_fifo(add);
      }
    }
    else
    {
      // Cache Miss!
      miss++;
    }
  }
}

int main(void)
{
  char opcode;
  long long int add;

  // Opening file as an input/read
  std::ofstream file;
  file.open("/* path goes here */", std::ios::in);
  // Reading the file inputs

  // Closing the file
  file.close();

  std::cout << "Hits: " << hit << std::endl;
  std::cout << "Miss: " << miss << std::endl;

  return 0;
}
