#!/usr/bin/perl


use File::Find;
use File::Basename;

print("Running find on $ARGV[0]");

find(
    {
        wanted => \&findfiles,
    },
    $ARGV[0]
);


sub findfiles
{
    $f = $File::Find::name;


    if ($f =~ m/\.h$/ or $f =~ m/.cpp$/)
    {
        print $f . "\n";

        $src = $_;
        open(IN, "< $_");
        open(OUT, "> $_.bak");
        while (<IN>)
        {
            # Replacement one. __m128 foo = bar -> auto foo = bar
            s/__m128 (\S+) =/auto $1 =/g;

            # Replacement two. Any other __m128 goes to SIMD_M128
            s/__m128([, ])/SIMD_M128$1/g;
            s/__m128i([, ])/SIMD_M128I$1/g;

            # Replacement three. Any _mm_foo_bar goes to SIMD_MM(foo_bar)
            s/_mm_([^\(]+)\(/SIMD_MM($1)(/g;

            s/_MM_SHUFFLE/SIMD_MM_SHUFFLE/g;
            print OUT;
        }
        close(OUT);
        close(IN);
        system("mv $src.bak $src");
        # die "foo";
    }
}