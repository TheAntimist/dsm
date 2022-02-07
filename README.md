# Distributed Shared Memory System

## Overview

This repository contains code on how to execute applications in a distributed shared memory system. Along with this we also provide code for applications of MapReduce
with word count and KMeans.

## File Structure

 - `directory.{h,cc}`: Contains code for the central directory.
 - `psu_dsm_system.{h,cpp}`: Contains the `psu_dsm_malloc` and `psu_dsm_register_datasegment` along with implentations of the Ricart Agarwala code.
 - `psu_lock.{h,cc}`: Exposes the `psu_init_lock`, `psu_mutex_lock`, `psu_mutex_unlock` implemented using Ricart Agarwala.
 - `psu_mr.{h,cpp}`: Contains the barrier functions and exposes `psu_mr_setup`, `psu_mr_map` and `psu_mr_reduce`
 - `wordcount.cpp`: Contains MapReduce Word Count Implementation.
 - `logger.h`: Contains Logger functions for writing to the file.
 - `kmeans.cpp`: Contains `map_kmeans` and `reduce_kmeans` functions.

## Building

To build the project one can run `make` which will invoke the build via `gcc` from the `Makefile`.

```sh
> make directory
```

## Execution

After building multiple executables will be generated.
