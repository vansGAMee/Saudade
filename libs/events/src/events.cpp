#include <saudade/events/event_types.hpp>
#include <saudade/events/event_block.hpp>
#include <saudade/events/event_queue.hpp>

// Explicit instantiation to ensure all template members compile cleanly under strict warnings
template class saudade::events::FixedEventBlock<512>;
template class saudade::events::SpscEventQueue<2048>;
