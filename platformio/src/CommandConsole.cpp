#include <string.h>
#include "CommandConsole.h"

void CommandConsole::init(void (*task_run)(CCTask *task))
{
    line_buf_len = 0;
    this->task_run = task_run;
};

void CommandConsole::read(uint8_t *data, uint32_t len)
{
    memcpy((char *)&line_buf + line_buf_len, data, len);
    line_buf_len += len;

    char *p_end = strstr((char *)&line_buf, "\r\n");
    while (p_end != NULL)
    {
        line_len = p_end - (char *)line_buf;
        line = (char *)&line_buf;

        CCTask *task = parseCmd(line);
        task_run(task);

        // move buf
        int remain = line_buf_len - line_len - 2;
        for (int i = 0; i < remain; i++)
        {
            line_buf[i] = line_buf[i + line_len + 2];
        }
        line_buf_len -= (line_len + 2);
        memset((char *)&line_buf + line_buf_len, 0, line_len + 2);
        p_end = strstr((char *)&line_buf, "\r\n");
    }
}

CCTask *CommandConsole::parseCmd(char *line)
{
    // Task Line Format: ${cmd}(arg1,arg2,...)
    
    CCTask *p_task = &task;

    memset(&p_task->cmd, 0, CCTASK_CMD_LEN);
    p_task->argc = 0;
    for (int i = 0; i < CCTASK_ARG_NUM; i++)
    {
        memset(&p_task->argv[i], 0, CCTASK_ARG_LEN);
    }

    char *p_search = strstr(line, "(");
    if (NULL == p_search)
        return p_task;

    char *p_search_arg = strstr(p_search, ")");
    if (NULL == p_search_arg)
        return p_task;

    memcpy(&p_task->cmd, line, p_search - line);

    // args
    char *p_arg_start = p_search + 1;
    char *p_arg_end;
    while ((p_arg_end = strstr(p_arg_start, ",")) != NULL)
    {
        if (p_task->argc >= CCTASK_ARG_NUM)
        {
            return p_task;
        }
        memcpy(&p_task->argv[p_task->argc], p_arg_start, (p_arg_end - p_arg_start));
        p_task->argc++;
        p_arg_start = p_arg_end + 1;
    }
    p_arg_end = strstr(p_arg_start, ")");
    if (p_arg_end != NULL && (p_arg_end - p_arg_start) >= 1)
    {
        memcpy(&p_task->argv[p_task->argc], p_arg_start, (p_arg_end - p_arg_start));
        p_task->argc++;
    }

    return p_task;
}