#pragma once

namespace Pumpkin
{
struct PropagationStatus
{
	PropagationStatus(bool conflict_detected) :conflict_detected(conflict_detected) {}
	bool conflict_detected;
};
}