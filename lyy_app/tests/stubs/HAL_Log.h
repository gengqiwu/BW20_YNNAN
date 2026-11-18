#ifndef TEST_HAL_LOG_H
#define TEST_HAL_LOG_H
static inline void test_log(const char *format, ...) { (void)format; }
#define LYY_LOGD(...) test_log(__VA_ARGS__)
#define LYY_LOGE(...) test_log(__VA_ARGS__)
#define LYY_LOGI(...) test_log(__VA_ARGS__)
#endif
