#include <stdint.h>

#define CCLINE_BUF_LEN 1024
#define CCTASK_CMD_LEN 30
#define CCTASK_ARG_NUM 10
#define CCTASK_ARG_LEN 30


typedef struct
{
    char cmd[CCTASK_CMD_LEN];
    uint8_t argc;
    char argv[CCTASK_ARG_NUM][CCTASK_ARG_LEN];
} CCTask;

class CommandConsole
{
public:
    void init(void (*task_run)(CCTask *task));
    void read(uint8_t *rx_buf, uint32_t rx_len);

protected:
    CCTask *parseCmd(char *line);

private:
    char line_buf[CCLINE_BUF_LEN];
    uint32_t line_buf_len;
    char *line;
    uint32_t line_len;
    CCTask task;
    void (*task_run)(CCTask *task);
};