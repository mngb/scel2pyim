# scel2pyim
一个将搜狗输入法scel细胞词库转换为emacs chinese-pyim文本词库的小工具。
支持目录输入，运行时会到 RootDir 下面递归查找所有 .scel 文件，生成一个大的 .pyim 文件。
### 使用：
	$ gcc -o scel2pyim scel2pyim.c
	$ ./scel2pyim RootDir NAME.pyim

