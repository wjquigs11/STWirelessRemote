#include "CommandStack.h"

bool CommandStack::push(int item)
{
    if (top >= (MAX - 1))
    {
        return false;
    }
    else
    {
        myStack[++top] = item;
        return true;
    }
}

int CommandStack::pop()
{
    if (top < 0)
    {
        return -1;
    }
    else
    {
        // FIFO Queue
        int item = myStack[0];
        top--;
        if (top >= 0)
        {
            updateArrayAfterPop();
        }
        return item;
    }
}

bool CommandStack::isEmpty()
{
    return (top < 0);
}

void CommandStack::updateArrayAfterPop()
{
    for (int i = 0; i <= top; i++)
    {
        myStack[i] = myStack[i + 1];
    }
}
