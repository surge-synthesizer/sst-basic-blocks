# sst-basic-blocks

`sst-basic-blocks` is our base level utility classes for things like parameters, block operations,
sse helpers, base level dsp, and more. It's the 'bottom of the stack' in the surge libraries.

## An important note about licensing

`sst-basic-blocks` is largely derived from refactoring surge and other properties which are GPL3
and as such this library is GPL3. A small number of individual files were developed in a context
where they are (1) standalone (or can be trivially made standalone by removing a header etc) and
(2) are useful-in and co-developed-with folks who use them in a non-GPL3 context. As such a few
individual header files are also available to use in an MIT license context. Those header files
are explicitly marked in the text of the header file and are listed here also

- include/sst/basic-blocks/dsp/LanczosResampler.h
- include/sst/basic-blocks/dsp/HilbertTransform.h