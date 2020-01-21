#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char addr;
    ctx.Low = StackBottom;
    ctx.Hight = &addr;
    if (ctx.Hight < ctx.Low) {
      std::swap(ctx.Hight, ctx.Low);
    }
    char *buf = std::get<0>(ctx.Stack);
    uint32_t size = std::get<1>(ctx.Stack);
    if (size < ctx.Hight - ctx.Low) {
        free(buf);
        buf = (char *)std::calloc(size, sizeof(char));
        size = ctx.Hight - ctx.Low;
    }
    memcpy(buf, ctx.Low, ctx.Hight - ctx.Low);
    ctx.Stack = std::make_tuple(buf, ctx.Hight - ctx.Low);
}

void Engine::Restore(context &ctx) {
  char addr;
  if ((&addr) >= ctx.Low && (&addr) < ctx.Hight) {
      Restore(ctx);
  }
  char *buf = std::get<0>(ctx.Stack);
  uint32_t size = std::get<1>(ctx.Stack);
  memcpy(ctx.Low, buf, size);
  cur_routine = &ctx;
  longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    if (!alive) {
      return;
    }
    if (alive == cur_routine) {
      if (alive->next) {
        Enter(*(alive->next));
      }
      return;
    }
    Enter(*alive);
}

void Engine::sched(void *routine) {
    if (cur_routine == routine) {
      return;
    }
    if (!routine) {
      yield();
    }
    Enter(*((context*)routine));
}

void Engine::Enter(context &ctx) {
    if (cur_routine && cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
          return;
        }
        Store(*cur_routine);
    }
    cur_routine = &ctx;
    Restore(ctx);
}

} // namespace Coroutine
} // namespace Afina
