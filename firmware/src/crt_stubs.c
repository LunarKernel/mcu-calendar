/* -nostartfiles 摘掉 crt0 时，crti/crtn 也一并没了，它们本来提供 _init/_fini；
 * 启动文件调用的 __libc_init_array 仍要调 _init，这里给空实现。
 * C 代码的静态构造全走 .init_array，不用 .init 段，空实现不丢东西。 */
void _init(void);
void _fini(void);

void _init(void) {}

void _fini(void) {}
