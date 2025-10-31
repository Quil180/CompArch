/*
 * Made by Yousef Awad in C++ using CMake for the compilation.
 * Yay!
 */
/*
 * The following is all of the return types and what they signify:
 * 0: Program ran succesfully
 * 1: Error was found in the program
 *
 */
#include <fstream>
#include <iostream>

// The following are set by the command line
struct cli_defines {
  int CACHE_SIZE;
  int NUM_SETS;
  int ASSOC;
  int BLOCK_SIZE = 64;
  int REPLACEMENT_POLICY; // 0 = LRU, 1 = FIFO
  int WRITE_POLICY;       // 0 = write-through, 1 = write-back
};

// dynamic cache structure variables below
struct dynamic_cache {
  long long int **tag;
  long long int **lru_pos;
  bool **dirty;
  bool **valid;
  long long int **fifo_counter;
  long long int global_counter = 0;
};


void init_cache(cli_defines *cli, dynamic_cache *cache)
{
  // ok so im allocating an array of pointers for each set
  cache->tag = new long long int *[cli->NUM_SETS];
  cache->lru_pos = new long long int *[cli->NUM_SETS];
  cache->dirty = new bool *[cli->NUM_SETS];
  cache->valid = new bool *[cli->NUM_SETS];
  cache->fifo_counter = new long long int *[cli->NUM_SETS];

  // and for each set we allocate arrays for all ways
  for (int i = 0; i < cli->NUM_SETS; i++)
  {
    cache->tag[i] = new long long int[cli->ASSOC];
    cache->lru_pos[i] = new long long int[cli->ASSOC];
    cache->dirty[i] = new bool[cli->ASSOC];
    cache->valid[i] = new bool[cli->ASSOC];
    cache->fifo_counter[i] = new long long int[cli->ASSOC];

    // FINALLY we initialize all the blocks in this set to default/empty state
    for (int j = 0; j < cli->ASSOC; j++)
    {
      cache->tag[i][j] = -1;
      cache->lru_pos[i][j] = j;
      cache->dirty[i][j] = false;
      cache->valid[i][j] = false;
      cache->fifo_counter[i][j] = -1;
    }
  }
}

// Free's all used memory
void cleanup(cli_defines *cli, dynamic_cache *cache)
{
  for (int i = 0; i < cli->NUM_SETS; i++)
  {
    delete[] cache->tag[i];
    delete[] cache->lru_pos[i];
    delete[] cache->dirty[i];
    delete[] cache->valid[i];
    delete[] cache->fifo_counter[i];
  }
  delete[] cache->tag;
  delete[] cache->lru_pos;
  delete[] cache->dirty;
  delete[] cache->valid;
  delete[] cache->fifo_counter;
}

void update_lru(long long int add, cli_defines *cli, dynamic_cache *cache)
{
  // breaking down a memory address into cache components
  int set = (add / cli->BLOCK_SIZE) % cli->NUM_SETS;
  long long int tag_accessing = add / cli->BLOCK_SIZE;

  // finding which way was accessed
  int accessed_way = -1;
  for (int i = 0; i < cli->ASSOC; i++)
  {
    if (cache->valid[set][i] && cache->tag[set][i] == tag_accessing)
    {
      accessed_way = i;
      break;
    }
  }

  if (accessed_way == -1)
  {
    return;
  }

  int old_position = cache->lru_pos[set][accessed_way];

  // movin down all da all blocks with position < old_position down by 1
  for (int i = 0; i < cli->ASSOC; i++)
  {
    if (cache->lru_pos[set][i] < old_position)
    {
      cache->lru_pos[set][i]++;
    }
  }

  // ok so im setting the accessed block to MRU (position 0)
  cache->lru_pos[set][accessed_way] = 0;
}

int find_lru_victim(int set, cli_defines *cli, dynamic_cache *cache)
{
  int victim = 0;
  int max_pos = cache->lru_pos[set][0];

  for (int i = 1; i < cli->ASSOC; i++)
  {
    if (cache->lru_pos[set][i] > max_pos)
    {
      max_pos = cache->lru_pos[set][i];
      victim = i;
    }
  }
  return victim;
}

int find_fifo_victim(int set, cli_defines *cli, dynamic_cache *cache)
{
  int victim = 0;
  long long int min_counter = cache->fifo_counter[set][0];

  for (int i = 1; i < cli->ASSOC; i++)
  {
    if (cache->fifo_counter[set][i] < min_counter)
    {
      min_counter = cache->fifo_counter[set][i];
      victim = i;
    }
  }
  return victim;
}

int find_invalid_block(int set, cli_defines *cli, dynamic_cache *cache)
{
  for (int i = 0; i < cli->ASSOC; i++)
  {
    if (!(cache->valid[set][i]))
    {
      return i;
    }
  }
  return -1;
}

void simulate_access(char opcode, long long int add, unsigned int* hit, unsigned int *miss, unsigned int *memory_writes, unsigned int *memory_reads, cli_defines *cli, dynamic_cache *cache)
{
  int set = (add / cli->BLOCK_SIZE) % cli->NUM_SETS;
  long long int tag_accessing = add / cli->BLOCK_SIZE;

  bool is_hit = false;
  int hit_way = -1;

  // checkin for cache hit
  for (int i = 0; i < cli->ASSOC; i++)
  {
    if (cache->valid[set][i] && tag_accessing == cache->tag[set][i])
    {
      // Cache hit!!
      (*hit)++;
      is_hit = true;
      hit_way = i;

      if (cli->REPLACEMENT_POLICY == 0) // LRU
      {
        update_lru(add, cli, cache);
      } 
      else // FIFO
      {
        // FIFO does nothing...
      }

      // handling the write
      if (opcode == 'W')
      {
        if (cli->WRITE_POLICY == 1)
        {
          // Write-back: mark dirty
          cache->dirty[set][i] = true;
        }
        else
        {
          // write-through!!! write to memory
          (*memory_writes)++;
        }
      }

      break;
    }
  }

  // handle cache miss
  if (!is_hit)
  {
    (*miss)++;
    (*memory_reads)++; // ALWAYS read from memory on miss

    int victim_way;

    // trying to find an invalid block first
    victim_way = find_invalid_block(set, cli, cache);

    // if theyre all valid, need to evict
    if (victim_way == -1)
    {
      if (cli->REPLACEMENT_POLICY == 0)
      {
        victim_way = find_lru_victim(set, cli, cache);
      }
      else
      {
        victim_way = find_fifo_victim(set, cli, cache);
      }

      // if im evicting a dirty block in write-back, write to memory
      if (cache->valid[set][victim_way] && cache->dirty[set][victim_way] && cli->WRITE_POLICY == 1)
      {
        (*memory_writes)++;
      }
    }

    // install that new block
    cache->tag[set][victim_way] = tag_accessing;
    cache->valid[set][victim_way] = true;

    // set dirty bit
    if (opcode == 'W' && cli->WRITE_POLICY == 1)
    {
      cache->dirty[set][victim_way] = true;
    }
    else
    {
      cache->dirty[set][victim_way] = false;
    }

    // write-through!!!: write to memory on write
    if (opcode == 'W' && cli->WRITE_POLICY == 0)
    {
      (*memory_writes)++;
    }

    // update replacement policy
    if (cli->REPLACEMENT_POLICY == 0)
    {
      update_lru(add, cli, cache);
    }
    else
    {
      cache->fifo_counter[set][victim_way] = (cache->global_counter)++;
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc != 6)
  {
    std::cerr
        << "Usage: ./SIM <CACHE_SIZE> <ASSOC> <REPLACEMENT> <WB> <TRACE_FILE>"
        << std::endl;
    return 1;
  }
  
  cli_defines cli;
  dynamic_cache cache;

  // parse arguments
  cli.CACHE_SIZE = atoi(argv[1]);
  cli.ASSOC = atoi(argv[2]);
  cli.REPLACEMENT_POLICY = atoi(argv[3]);
  cli.WRITE_POLICY = atoi(argv[4]);
  const char *trace_file = argv[5];

  // statistic variables
  unsigned int hit = 0;
  unsigned int miss = 0;
  unsigned int memory_reads = 0;
  unsigned int memory_writes = 0;

  // calculate number of sets
  cli.NUM_SETS = cli.CACHE_SIZE / (cli.BLOCK_SIZE * cli.ASSOC);

  // self explanatory lulz
  init_cache(&cli, &cache);

  char opcode;
  long long int add;

  // Opening file for reading
  std::ifstream file;
  file.open(trace_file);

  if (!file.is_open())
  {
    std::cerr << "Error: Could not open the trace file." << std::endl;
    cleanup(&cli, &cache);
    return 1;
  }

  // Reading the file inputs
  while (file >> opcode >> std::hex >> add)
  {
    simulate_access(opcode, add, &hit, &miss, &memory_writes, &memory_reads, &cli, &cache);
  }

  // Closing the file
  file.close();

  // Calculate miss ratio
  int total = hit + miss;
  double miss_ratio = (total > 0) ? (double)miss / total : 0.0;

  // Print statistics
  std::cout << "Miss ratio: "  << miss_ratio << std::endl;
  std::cout << "write " << memory_writes << std::endl;
  std::cout << "read " << memory_reads << std::endl;

  cleanup(&cli, &cache);

  return 0;
}
