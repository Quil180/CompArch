/*
 * Made by Yousef Awad in C++ using CMake for the compilation.
 * Yay!
 */
/*
 * The following is all of the return types and what they signify:
 * 0: Program ran succesfully
 * 1: Error opening file
 *
 */
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

using namespace std; // stupid std flags bruh

// ok so you should set these from the command line
int NUM_SETS;
int ASSOC;
int BLOCK_SIZE = 64;
int REPLACEMENT_POLICY; // 0 = LRU, 1 = FIFO
int WRITE_POLICY;       // 0 = write-through, 1 = write-back

// dynamic cache structure crappp
long long int **tag;
long long int **lru_pos;
bool **dirty;
bool **valid;
long long int **fifo_counter;
long long int global_counter = 0;

// statistic variables
unsigned int hit = 0;
unsigned int miss = 0;
unsigned int memory_reads = 0;
unsigned int memory_writes = 0;

void init_cache() {
  // ok so im allocating an array of pointers for each set
  tag = new long long int *[NUM_SETS];
  lru_pos = new long long int *[NUM_SETS];
  dirty = new bool *[NUM_SETS];
  valid = new bool *[NUM_SETS];
  fifo_counter = new long long int *[NUM_SETS];

  // and for each set we allocate arrays for all ways
  for (int i = 0; i < NUM_SETS; i++) {
    tag[i] = new long long int[ASSOC];
    lru_pos[i] = new long long int[ASSOC];
    dirty[i] = new bool[ASSOC];
    valid[i] = new bool[ASSOC];
    fifo_counter[i] = new long long int[ASSOC];

    // FINALLY we initialize all the blocks in this set to default/empty state
    for (int j = 0; j < ASSOC; j++) {
      tag[i][j] = -1;
      lru_pos[i][j] = j;
      dirty[i][j] = false;
      valid[i][j] = false;
      fifo_counter[i][j] = -1;
    }
  }
}

// self explanatory..
void cleanup() {
  for (int i = 0; i < NUM_SETS; i++) {
    delete[] tag[i];
    delete[] lru_pos[i];
    delete[] dirty[i];
    delete[] valid[i];
    delete[] fifo_counter[i];
  }
  delete[] tag;
  delete[] lru_pos;
  delete[] dirty;
  delete[] valid;
  delete[] fifo_counter;
}

void update_lru(long long int add) {
  // breaking down a memory address into cache components
  int set = (add / BLOCK_SIZE) % NUM_SETS;
  long long int tag_accessing = add / BLOCK_SIZE;

  // finding which way was accessed
  int accessed_way = -1;
  for (int i = 0; i < ASSOC; i++) {
    if (valid[set][i] && tag[set][i] == tag_accessing) {
      accessed_way = i;
      break;
    }
  }

  if (accessed_way == -1)
    return;

  int old_position = lru_pos[set][accessed_way];

  // movin down all da all blocks with position < old_position down by 1
  for (int i = 0; i < ASSOC; i++) {
    if (lru_pos[set][i] < old_position) {
      lru_pos[set][i]++;
    }
  }

  // ok so im setting the accessed block to MRU (position 0)
  lru_pos[set][accessed_way] = 0;
}

void update_fifo(long long int add) {
  // FIFO doesn't update on hits, only on insertions lol
  // Literally nothing to do here lmao
}

int find_lru_victim(int set) {
  int victim = 0;
  int max_pos = lru_pos[set][0];

  for (int i = 1; i < ASSOC; i++) {
    if (lru_pos[set][i] > max_pos) {
      max_pos = lru_pos[set][i];
      victim = i;
    }
  }
  return victim;
}

int find_fifo_victim(int set) {
  int victim = 0;
  long long int min_counter = fifo_counter[set][0];

  for (int i = 1; i < ASSOC; i++) {
    if (fifo_counter[set][i] < min_counter) {
      min_counter = fifo_counter[set][i];
      victim = i;
    }
  }
  return victim;
}

int find_invalid_block(int set) {
  for (int i = 0; i < ASSOC; i++) {
    if (!valid[set][i]) {
      return i;
    }
  }
  return -1;
}

void simulate_access(char opcode, long long int add) {
  int set = (add / BLOCK_SIZE) % NUM_SETS;
  long long int tag_accessing = add / BLOCK_SIZE;

  bool is_hit = false;
  int hit_way = -1;

  // checkin for cache hit
  for (int i = 0; i < ASSOC; i++) {
    if (valid[set][i] && tag_accessing == tag[set][i]) {
      // Cache hit!!
      hit++;
      is_hit = true;
      hit_way = i;

      if (REPLACEMENT_POLICY == 0) // LRU
      {
        update_lru(add);
      } else // FIFO
      {
        update_fifo(add);
      }

      // handling the write
      if (opcode == 'W') {
        if (WRITE_POLICY == 1) {
          // Write-back: mark dirty
          dirty[set][i] = true;
        } else {
          // write-through!!! write to memory
          memory_writes++;
        }
      }

      break;
    }
  }

  // handle cache miss
  if (!is_hit) {
    miss++;
    memory_reads++; // ALWAYS read from memory on miss

    int victim_way;

    // trying to find an invalid block first
    victim_way = find_invalid_block(set);

    // if theyre all valid, need to evict
    if (victim_way == -1) {
      if (REPLACEMENT_POLICY == 0) {
        victim_way = find_lru_victim(set);
      } else {
        victim_way = find_fifo_victim(set);
      }

      // if im evicting a dirty block in write-back, write to memory
      if (valid[set][victim_way] && dirty[set][victim_way] &&
          WRITE_POLICY == 1) {
        memory_writes++;
      }
    }

    // install that new block
    tag[set][victim_way] = tag_accessing;
    valid[set][victim_way] = true;

    // set dirty bit
    if (opcode == 'W' && WRITE_POLICY == 1) {
      dirty[set][victim_way] = true;
    } else {
      dirty[set][victim_way] = false;
    }

    // write-through!!!: write to memory on write
    if (opcode == 'W' && WRITE_POLICY == 0) {
      memory_writes++;
    }

    // update replacement policy
    if (REPLACEMENT_POLICY == 0) {
      update_lru(add);
    } else {
      fifo_counter[set][victim_way] = global_counter++;
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 6) {
    std::cerr
        << "Usage: ./SIM <CACHE_SIZE> <ASSOC> <REPLACEMENT> <WB> <TRACE_FILE>"
        << std::endl;
    return 1;
  }

  // parse arguments
  int CACHE_SIZE = atoi(argv[1]);
  ASSOC = atoi(argv[2]);
  REPLACEMENT_POLICY = atoi(argv[3]);
  WRITE_POLICY = atoi(argv[4]);
  const char *trace_file = argv[5];

  // calculate number of sets
  NUM_SETS = CACHE_SIZE / (BLOCK_SIZE * ASSOC);

  // self explanatory lulz
  init_cache();

  char opcode;
  long long int add;

  // Opening file for reading
  std::ifstream file;
  file.open(trace_file);

  if (!file.is_open()) {
    std::cerr << "Error: Could not open the trace file." << std::endl;
    cleanup();
    return 1;
  }

  // Reading the file inputs
  while (file >> opcode >> std::hex >> add) {
    simulate_access(opcode, add);
  }

  // Closing the file
  file.close();

  // Calculate miss ratio
  int total = hit + miss;
  double miss_ratio = (total > 0) ? (double)miss / total : 0.0;

  // Print statistics
  printf("Miss ratio %.6f\n", miss_ratio);
  printf("write %u\n", memory_writes);
  printf("read %u\n", memory_reads);

  cleanup();

  return 0;
}
