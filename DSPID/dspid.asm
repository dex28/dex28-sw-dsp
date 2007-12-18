      .sect ".text"
      .def _c_int00

_c_int00:

      LD   *(0x003E), A
      STL  A, *(0x1100)

L1:    
      B     L1

      .end
