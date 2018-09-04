Benchmark division
==================

This benchmark compares performance of the frac and div modules against the
compiler generated division code.
An array of TEST_NUMOF pseudorandom numbers is used as the input to the scaling
functions.
The same scale factor is applied using three different methods: traditional
division operator (z = x / y), the frac module (z = frac_scale(frac, x)), and
the div module (where applicable, e.g. z = div_u64_by_1000000(x))
One test uses the constant 512/15625 scaling factor used by
xtimer_ticks_from_usec when using a 32768 Hz timer. A constant scaling factor
may receive extra optimization in the compiler, so the benchmark additionally
provides two more tests of a varying denominator and a varying numerator.

Results
=======

The output shows the execution time (in usec by default) required for scaling
TEST_NUMOF number of pseudorandom uin64_t values with a the same scaling factor.
