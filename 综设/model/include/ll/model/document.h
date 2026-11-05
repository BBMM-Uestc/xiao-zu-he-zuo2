// owner: D  stability: frozen
#pragma once

#include "ll/model/circuit.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ll {

class Document {
public:
    Document();
    explicit Document(std::string main_circuit);

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;
    Document(Document&&) noexcept;
    Document& operator=(Document&&) noexcept;
    ~Document();

    static Document CreateEmpty(std::string main_circuit = "main");

    Circuit&       Main();
    const Circuit& Main() const;

    Circuit*       FindCircuit(std::string_view name);
    const Circuit* FindCircuit(std::string_view name) const;
    Circuit&       CreateCircuit(std::string_view name);
    bool           RemoveCircuit(std::string_view name);  // "main" cannot be removed

    const std::vector<std::unique_ptr<Circuit>>& Circuits() const { return circuits_; }

    std::uint64_t Revision() const { return revision_; }
    bool          Dirty() const { return dirty_; }
    void          MarkSaved() { dirty_ = false; }

    Status Validate() const;

private:
    Circuit& AddCircuit(std::string name);
    void     RebindCircuits();

    std::string main_name_;
    std::vector<std::unique_ptr<Circuit>> circuits_;
    std::uint64_t revision_ = 0;
    bool dirty_ = false;
    CircuitId next_circuit_id_ = 1;
};

}  // namespace ll
