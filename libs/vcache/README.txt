vcacheopt - Vertex Cache Optimizer documentation

This library is an implementation of the "Linear-Speed Vertex Cache Optimisation" algorithm. You can find the algorithm description at:

http://home.comcast.net/~tom_forsyth/papers/fast_vert_cache_opt.html

The library consists of a single header file "vcacheopt.h" and is written in C++. To use it, just #include the file in your C++ source. test.cpp is a simple test program that creates an indexed mesh, optimizes it and reports statistics (cache misses and ACMR before and after the optimization and the time required).

The code is tested with visual studio, but it must work with any c++ compiler.

There are 2 classes that you can use:

- class VertexCacheOptimizer:

This is the main class for optimizing indexed meshes. The index buffer to be optimized must be a int array holding 3*n indices, where n is the triangle count of the mesh. To optimize such a buffer do the following:

// create an optimizer object
VertexCacheOptimizer vcache;

// perform optimization. n is the triangle count, *not* the index count
VertexCacheOptimizer::Result r = vcache.Optimize(index_buffer, n);

If the function returns Success (=zero), the optimization is complete and the result is stored back in your buffer. If the function fails, it will return either Fail_BadIndex, which means that one or more of your indices is negative or Fail_NoVerts, which means that your index count is zero (no indices to optimize).

- class VertexCache

This class emulates a vertex cache with 40 positions. You can use it to evaluate your resulting optimized meshes to check if the new cache miss count is indeed lower than the previews one of the unoptimized mesh:

VertexCache cache;
int miss_count = cache.GetCacheMissCount(index_buffer, n);

You can do this before and after optimization to compare cache misses before and after.

Feedback is most welcome at <mgeorgoulopoulos at gmail>.

---
2009 Michael Georgoulopoulos
