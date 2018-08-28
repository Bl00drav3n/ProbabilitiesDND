#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

struct samples {
    u32 Count;
    u32 *Buckets;

    u32 MinValue;
    u32 MaxValue;
};

struct sample_stats {
    f64 Mean;
    f64 Var;
    f64 Std;
};

sample_stats GetStatistics(samples Samples, u32 NumTries) {
    u64 Agg = 0;
    for(u32 i = 0; i < Samples.Count; i++) {
        Agg += (Samples.MinValue + i) * Samples.Buckets[i];
    }
    f64 Mean = (f64)Agg / (f64)NumTries;

    f64 VarSqrAgg = 0;
    for(u32 i = 0; i < Samples.Count; i++) {
        f64 Diff = (Samples.MinValue + i) - Mean;
        VarSqrAgg += (f64)Samples.Buckets[i] * (Diff * Diff);
    }
    f64 Var = VarSqrAgg / ((f64)NumTries - 1);
    f64 Std = sqrtf(Var);

    sample_stats Stats = {};
    Stats.Mean = Mean;
    Stats.Var = Var;
    Stats.Std = Std;

    return Stats;
}

f64 GetRandomUnilateral() {
    return (f64)rand() / (1.0 + (f64)(RAND_MAX));
}

u32 GetRandomInRange(u32 Min, u32 Max) {
    assert(Min < Max);
    return Min + ((f64)(Max - Min + 1)) * GetRandomUnilateral();
}

void FreeSamples(samples *Samples) {
    free(Samples->Buckets);
}

f32 GetPercentage(u32 Value, u32 Max) {
    return 100.f * (f32)Value / (f32)Max;
}

u32 RollFairDice(u32 DiceCount, u32 DiceSideCount) {
    u32 Result = 0;
    for(u32 Die = 0; Die < DiceCount; Die++) {
        Result += GetRandomInRange(1, DiceSideCount);
    }
    return Result;
}

#define ROLLS_SCRATCH_BUFFER_SIZE 64
u32 RollFairDiceDiscardLowest(u32 DiceCount, u32 DiceSideCount) {
    u32 Result = 0;
    u32 MinIdx = 0xffffffff;
    u32 Min = 0xffffffff;
    u32 RollsBuffer[ROLLS_SCRATCH_BUFFER_SIZE];
    u32 *Rolls = 0;

    if(DiceCount < ROLLS_SCRATCH_BUFFER_SIZE) {
        Rolls = RollsBuffer;
    }
    else {
        Rolls = (u32*)calloc(DiceCount, sizeof(u32));
    }
    
    for(u32 Roll = 0; Roll < DiceCount; Roll++) {
        Rolls[Roll] = GetRandomInRange(1, DiceSideCount);
        if(Rolls[Roll] < Min) {
            Min = Rolls[Roll];
            MinIdx = Roll;
        }
    }

    assert(MinIdx < DiceCount);

    for(u32 Roll = 0; Roll < DiceCount; Roll++) {
        if(Roll != MinIdx) {
            Result += Rolls[Roll];
        }
    }
    
    if(Rolls != RollsBuffer) {
        free(Rolls);
    }

    return Result;
}

samples SampleDiceRolls(u32 DiceCount, u32 DiceSideCount, u32 SamplePopCount, bool DiscardLowestRoll)
{
    samples Samples = {};
    if(DiscardLowestRoll) {
        Samples.MinValue = DiceCount - 1;
        Samples.MaxValue = (DiceCount - 1) * DiceSideCount;
    }
    else {
        Samples.MinValue = DiceCount;
        Samples.MaxValue = DiceCount * DiceSideCount;
    }
    Samples.Count = Samples.MaxValue - Samples.MinValue + 1;
    Samples.Buckets = (u32*)calloc(Samples.Count, sizeof(u32));

    for(u32 i = 0; i < SamplePopCount; i++) {
        u32 Agg;
        if(DiscardLowestRoll) {
            Agg = RollFairDiceDiscardLowest(DiceCount, DiceSideCount);
        }
        else {
            Agg = RollFairDice(DiceCount, DiceSideCount);
        }
        u32 Bucket = Agg - Samples.MinValue;
        assert(Bucket < Samples.Count);
        Samples.Buckets[Bucket]++;
    }

    return Samples;
}

int main(int argc, char *argv[]) {
    srand(time(0));

    if(argc != 5) {
        printf("   Usage: prob.exe DiceCount DiceSideCount SamplePopCount Discard\n"
               "          Pass all as integers and Discard as true/false");
        return 0;
    }

#if 1
    bool Discard = strcmp("true", argv[--argc]) == 0;
    u32 SamplePopCount = (u32)atoi(argv[--argc]);
    u32 DiceSideCount = (u32)atoi(argv[--argc]);
    u32 DiceCount = (u32)atoi(argv[--argc]);
#else
    bool Discard = true;
    u32 DiceCount = 4;
    u32 DiceSideCount = 6;
    u32 SamplePopCount = 1000000;
#endif
    samples Samples = SampleDiceRolls(DiceCount, DiceSideCount, SamplePopCount, Discard);
    
    printf("Distribution of %dd%d using %d throws:\n", DiceCount, DiceSideCount, SamplePopCount);
    printf("\nSampling:\n");
    for(u32 i = 0; i < Samples.Count; i++) {
        printf("%d ", Samples.Buckets[i]);
    }
    printf("\nDistribution:\n");
    for(u32 i = 0; i < Samples.Count; i++) {
        printf("%.1f%% ", GetPercentage(Samples.Buckets[i], SamplePopCount));
    }
    printf("\n\nStatistics:\n");
    sample_stats Stats = GetStatistics(Samples, SamplePopCount);
    printf("   mean: %.1lf   var: %.1lf   std: %.1lf\n", Stats.Mean, Stats.Var, Stats.Std);

    FILE *file = fopen("out.txt", "w");
    for(u32 i = 0; i < Samples.Count; i++) {
        fprintf(file, "%d %.2f\n", Samples.MinValue + i, GetPercentage(Samples.Buckets[i], SamplePopCount));
    }
    fclose(file);

    FreeSamples(&Samples);

    return 0;
}