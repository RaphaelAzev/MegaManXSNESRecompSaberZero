#pragma once
#include <stddef.h>
struct RecompLauncherCGameInfo;
#ifdef __cplusplus
extern "C" {
#endif
int MmxNetplayActive(void);
void MmxNetplayConfigureLauncher(struct RecompLauncherCGameInfo *info);
int MmxNetplayPrepare(int from_lobby, char *reason, size_t cap);
int MmxNetplayReady(char *reason, size_t cap);
#ifdef __cplusplus
}
#endif
