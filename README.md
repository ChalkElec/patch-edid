patch-edid
==========

Created by Dr. Ace Jeangle (support@chalk-elec.com)

Patches EDID information in Intel HEX firmware 

You can compile source with standard gcc compiler: 
```
gcc main.c -o patch-edid
```
Example
==========
```
patch-edid ./7-of-mt.hex ./1920x1200_edid.bin
```
will update EDID data in 7-of-mt.hex firmware with data from 1920x1200_edid.bin file
