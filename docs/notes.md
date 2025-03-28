反编译 csi_decoder 得知

`csi_vdev_open` 的第二个参数可以填 0。但是那明明是个 `const char *` 为什么呢？