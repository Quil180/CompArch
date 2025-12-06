#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct
{
    uint8_t *pht;
    uint32_t ghr;
    int M;
    int N;
} Predictor;

int init_predictor(Predictor *pred, int m, int n)
{
    if (m < 0 || m > 30) // Prevent excessive allocation or negative shifts
    {
        return 0;
    }
    if (n < 0 || n > m) // N must be <= M
    {
        return 0;
    }

    pred->M = m;
    pred->N = n;
    size_t pht_size = 1U << pred->M;
    pred->pht = (uint8_t *)malloc(pht_size * sizeof(uint8_t));
    
    if (pred->pht == NULL)
    {
        return 0; // Return 0 if memory allocation fails
    }

    // Initialize to Weakly Taken
    memset(pred->pht, 2, pht_size);
    // GHR is set to 0
    pred->ghr = 0;
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Usage: %s gshare <GPB> <RB> <Trace_File>\n", argv[0]);
        printf("1) GPB: The number of PC bits used to index the gshare table.\n");
        printf("2) RB: The global history register bits used to index the gshare table.\n");
        printf("3) Trace_File: The trace file name along with its extension.\n");
        return 1;
    }

    if (strcmp(argv[1], "gshare") != 0)
    {
        printf("Error: Only 'gshare' predictor is supported. Usage: %s gshare <GPB> <RB> <Trace_File>\n", argv[0]);
        return 1;
    }

    char *endptr;
    long m_long = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || m_long < 0 || m_long > 30)
    {
      printf("Error: Invalid or out-of-range value for GPB (M): %s\n", argv[2]);
      return 1;
    }
    // M was safely parsed
    int m = (int)m_long;

    long n_long = strtol(argv[3], &endptr, 10);
    if (*endptr != '\0' || n_long < 0 || n_long > m)
    {
      printf("Error: Invalid or out-of-range value for RB (N): %s\n", argv[3]);
      return 1;
    }
    // N was safely parsed
    int n = (int)n_long;

    char *trace_file = argv[4];

    Predictor pred;
    pred.pht = NULL; // Ensuring pointer is null and not garbage value.
    if (!init_predictor(&pred, m, n))
    {
        // Return 0 if initialization fails
        printf("Error: Invalid parameters or memory allocation failure (M=%d, N=%d)\n", m, n);
        return 1;
    }

    unsigned long long total_branches = 0;
    unsigned long long mispredictions = 0;

    FILE *fp = fopen(trace_file, "r");
    if (fp == NULL)
    {
        // Return 0 if file opening fails
        printf("Error opening file %s\n", trace_file);
        free(pred.pht);
        return 1;
    }

    char line[256];
    // Read each line of the trace file
    while (fgets(line, sizeof(line), fp))
    {
        unsigned int pc;
        char outcome_char;
        char outcome_str[10];

        if (sscanf(line, "%x %9s", &pc, outcome_str) != 2)
        {
            // Skip invalid lines
            continue;
        }
        outcome_char = outcome_str[0];
        
        int actual_taken = (outcome_char == 't');

        // Index Calculation
        // PC index: bits M+1 to 2
        // We need to mask PC to get the relevant bits.
        // But simply shifting right by 2 and masking with (1<<M)-1 gives the M bits.
        unsigned int pc_index = (pc >> 2) & ((1U << pred.M) - 1);
        
        // Index Calculation:
        // XOR the N-bit GHR with the upper N bits of the M-bit PC index.
        unsigned int index = pc_index ^ (pred.ghr << (pred.M - pred.N));
        
        // Prediction
        int counter = pred.pht[index];
        int prediction_taken = (counter >= 2);
        
        if (prediction_taken != actual_taken)
        {
            // Increment mispredictions if prediction is incorrect
            mispredictions++;
        }
        
        total_branches++;
        
        // Update PHT
        if (actual_taken)
        {
            // Increment counter if prediction is correct
            if (counter < 3)
            {
                pred.pht[index]++;
            }
        }
        else
        {
            // Decrement counter if prediction is incorrect
            if (counter > 0)
            {
                pred.pht[index]--;
            }
        }
        
        // Update GHR
        if (pred.N > 0)
        {
            // Shift right 1, insert new outcome at MSB (bit N-1).
            pred.ghr = (pred.ghr >> 1) | (actual_taken << (pred.N - 1));
        }
    }

    // Close the trace file
    fclose(fp);

    // Print the misprediction rate
    printf("%d %d %.2f\n", m, n, (double)mispredictions / total_branches);

    // Free the PHT
    free(pred.pht);

    // Returning 0 since the program executed successfully
    return 0;
}
