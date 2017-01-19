#include <pce/pce.hpp>

namespace PCEngine {

CPU cpu;
#include "io.cpp"
#include "irq.cpp"
#include "timer.cpp"

auto CPU::Enter() -> void {
  while(true) scheduler.synchronize(), cpu.main();
}

auto CPU::main() -> void {
  if(irq.pending()) return interrupt(irq.vector());
  instruction();
}

auto CPU::step(uint clocks) -> void {
  Thread::step(clocks);
  timer.step(clocks);
  synchronize(vdc);
  synchronize(psg);
  for(auto peripheral : peripherals) synchronize(*peripheral);
}

auto CPU::power() -> void {
  HuC6280::power();
  create(CPU::Enter, system.colorburst() * 2.0);

  r.pc.byte(0) = read(0x1ffe);
  r.pc.byte(1) = read(0x1fff);

  memory::fill(&irq, sizeof(IRQ));
  memory::fill(&timer, sizeof(Timer));
}

auto CPU::lastCycle() -> void {
  irq.poll();
}

}