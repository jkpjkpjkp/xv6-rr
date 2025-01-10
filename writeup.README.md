正在绝赞debug中。目前已知的bug（行为）列表见末尾




这个代码没有用xv6-k210(因为笔者没用明白)，而是fork了**xv6-riscv**。

那么如何处理fat32 sdcard.img呢？
我在用户态调fatfs，先把sdcard的/内文件全部拷进/sdcard，然后执行测试（见
    fat32/   （这是fatfs，接入virtio blk后端）
    user/utests.c   （user/init.c 不再exec sh，而是exec这玩意。它负责两个disk间文件拷贝）
）
一共拷大概2M的文件

为了支持两个盘还做了两个virtiodisk的扩展，大概相当于mit 6.1810的某个lab



之后比较routine，比如fs.c加入了namefd，和namex类似但支持testcase中相对于fd的索引要求




因为corner case比较多，没有支持
    mount
    mmap


听起来做的事情确实不多，但为了支持fat32读/改的代码也基本是xv6的全部了。可以看到散落在各处被//掉的printf





目前的bug：
    shutdown。因为没有sbi，得研究怎么设mvec。还没搞
    
    brk、clone因为不明原因一上来就usertrap

    test_echo在被execve的时候读不了（拷贝的log是正常的）

    exit不了（很迷惑，好像sys_exit都没执行）




别的基本正常。


