# newLearnedFTL
目前正在找实习，准备工作。
这个项目可以先参考 https://github.com/astlxmu/LearnedFTL 这个项目，在这个项目的基础上用本项目中的bbssd替换/LearnedFTL/hw/femu/文件夹下的bbssd文件夹中的内容即可。

我现在把我论文中的结果和命令给上传了上去，请注意以下几点：
1. 我使用的ubuntu主机版本是18.04 虚拟机也是ubuntu版本为18.04
2. 如果你参考 https://github.com/astlxmu/LearnedFTL/tree/main 这个项目 你在编译的之前请确保把`LearnedFTL/blob/main/hw/femu/meson.build`这个文件中的`'bbssd/bb.c', 'bbssd/ld-tpftl.c'`对应的内容给修改为你所要编译的 .c文件（例如：`'bbssd/bb.c','bbssd/Segtable.c'`等）并用本项目中的bbssd目录替换https://github.com/astlxmu/LearnedFTL/tree/main 这个项目中`LearnedFTL/blob/main/hw/femu/bbssd`
3. 关于bbssd文件夹下的文件其中`Segtable.h` 和 `Segtable.c`文件是论文https://dl.acm.org/doi/abs/10.1145/3799988 中的完整实现，其余的是一些baseline和消融对比。
4. 另外为了方便复现论文中的实验，我把实验中的所有一些命令和结果都放到了[result](./result/)下。因为我把电脑的ubuntu系统给删除了，导致我之前的环境不存在了，所以关于Filebench的Workloads本项目中并没有提供，但是你可以参考论文中的参数进行修改，rocksdb可能提供了对应的命令（你在结果中应该能找到我当时设置一些参数一边方便您复现），最后我所有的fio命令在[result](./result/)中应该都可以找到。