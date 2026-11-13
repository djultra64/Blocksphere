/* Use the original x86-64 glibc ABI for sinh. glibc 2.44 adds a new default. */
extern double tetrisphere_glibc_sinh(double value);
__asm__(".symver tetrisphere_glibc_sinh,sinh@GLIBC_2.2.5");

double __wrap_sinh(double value)
{
    return tetrisphere_glibc_sinh(value);
}
