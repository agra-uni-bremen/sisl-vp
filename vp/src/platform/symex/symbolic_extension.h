#ifndef RISCV_ISA_SYMBOLIC_VALUE_H
#define RISCV_ISA_SYMBOLIC_VALUE_H

#include <clover/clover.h>

class SymbolicExtension : public tlm::tlm_extension<SymbolicExtension> {
	std::shared_ptr<clover::ConcolicValue> value;

public:
	typedef tlm::tlm_base_protocol_types::tlm_payload_type tlm_payload_type;
	typedef tlm::tlm_base_protocol_types::tlm_phase_type tlm_phase_type;

	SymbolicExtension(std::shared_ptr<clover::ConcolicValue> _value) {
		value = _value;
	}

	~SymbolicExtension(void) {
		return; // TODO: Decrement shared_ptr counter?
	}

	void copy_from(const tlm_extension_base &extension) {
		value = static_cast<SymbolicExtension const &>(extension).value;
	}

	tlm::tlm_extension_base *clone(void) const {
		return new SymbolicExtension(*this);
	}

	std::shared_ptr<clover::ConcolicValue> getValue(void) {
		return value;
	}
};

#endif