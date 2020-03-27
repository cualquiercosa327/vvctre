// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <boost/container/static_vector.hpp>
#include <boost/range/algorithm/fill.hpp>
#include <nihstro/shader_bytecode.h>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/vector_math.h"
#include "video_core/pica_state.h"
#include "video_core/pica_types.h"
#include "video_core/shader/shader.h"
#include "video_core/shader/shader_interpreter.h"

using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace Pica::Shader {

struct CallStackElement {
    u32 final_address;  // Address upon which we jump to return_address
    u32 return_address; // Where to jump when leaving scope
    u8 repeat_counter;  // How often to repeat until this call stack element is removed
    u8 loop_increment;  // Which value to add to the loop counter after an iteration
                        // TODO: Should this be a signed value? Does it even matter?
    u32 loop_address;   // The address where we'll return to after each loop iteration
};

static void RunInterpreter(const ShaderSetup& setup, UnitState& state, unsigned offset) {
    // TODO: Is there a maximal size for this?
    boost::container::static_vector<CallStackElement, 16> call_stack;
    u32 program_counter = offset;

    state.conditional_code[0] = false;
    state.conditional_code[1] = false;

    auto call = [&program_counter, &call_stack](u32 offset, u32 num_instructions, u32 return_offset,
                                                u8 repeat_count, u8 loop_increment) {
        // -1 to make sure when incrementing the PC we end up at the correct offset
        program_counter = offset - 1;
        ASSERT(call_stack.size() < call_stack.capacity());
        call_stack.push_back(
            {offset + num_instructions, return_offset, repeat_count, loop_increment, offset});
    };

    auto evaluate_condition = [&state](Instruction::FlowControlType flow_control) {
        using Op = Instruction::FlowControlType::Op;

        bool result_x = flow_control.refx.Value() == state.conditional_code[0];
        bool result_y = flow_control.refy.Value() == state.conditional_code[1];

        switch (flow_control.op) {
        case Op::Or:
            return result_x || result_y;
        case Op::And:
            return result_x && result_y;
        case Op::JustX:
            return result_x;
        case Op::JustY:
            return result_y;
        default:
            UNREACHABLE();
            return false;
        }
    };

    const auto& uniforms = setup.uniforms;
    const auto& swizzle_data = setup.swizzle_data;
    const auto& program_code = setup.program_code;

    // Placeholder for invalid inputs
    static float24 dummy_vec4_float24[4];

    unsigned iteration = 0;
    bool exit_loop = false;
    while (!exit_loop) {
        if (!call_stack.empty()) {
            auto& top = call_stack.back();
            if (program_counter == top.final_address) {
                state.address_registers[2] += top.loop_increment;

                if (top.repeat_counter-- == 0) {
                    program_counter = top.return_address;
                    call_stack.pop_back();
                } else {
                    program_counter = top.loop_address;
                }

                // TODO: Is "trying again" accurate to hardware?
                continue;
            }
        }

        const Instruction instr = {program_code[program_counter]};
        const SwizzlePattern swizzle = {swizzle_data[instr.common.operand_desc_id]};

        auto LookupSourceRegister = [&](const SourceRegister& source_reg) -> const float24* {
            switch (source_reg.GetRegisterType()) {
            case RegisterType::Input:
                return &state.registers.input[source_reg.GetIndex()].x;

            case RegisterType::Temporary:
                return &state.registers.temporary[source_reg.GetIndex()].x;

            case RegisterType::FloatUniform:
                return &uniforms.f[source_reg.GetIndex()].x;

            default:
                return dummy_vec4_float24;
            }
        };

        switch (instr.opcode.Value().GetInfo().type) {
        case OpCode::Type::Arithmetic: {
            const bool is_inverted =
                (0 != (instr.opcode.Value().GetInfo().subtype & OpCode::Info::SrcInversed));

            const int address_offset =
                (instr.common.address_register_index == 0)
                    ? 0
                    : state.address_registers[instr.common.address_register_index - 1];

            const float24* src1_ = LookupSourceRegister(instr.common.GetSrc1(is_inverted) +
                                                        (is_inverted ? 0 : address_offset));
            const float24* src2_ = LookupSourceRegister(instr.common.GetSrc2(is_inverted) +
                                                        (is_inverted ? address_offset : 0));

            const bool negate_src1 = ((bool)swizzle.negate_src1 != false);
            const bool negate_src2 = ((bool)swizzle.negate_src2 != false);

            float24 src1[4] = {
                src1_[(int)swizzle.src1_selector_0.Value()],
                src1_[(int)swizzle.src1_selector_1.Value()],
                src1_[(int)swizzle.src1_selector_2.Value()],
                src1_[(int)swizzle.src1_selector_3.Value()],
            };
            if (negate_src1) {
                src1[0] = -src1[0];
                src1[1] = -src1[1];
                src1[2] = -src1[2];
                src1[3] = -src1[3];
            }
            float24 src2[4] = {
                src2_[(int)swizzle.src2_selector_0.Value()],
                src2_[(int)swizzle.src2_selector_1.Value()],
                src2_[(int)swizzle.src2_selector_2.Value()],
                src2_[(int)swizzle.src2_selector_3.Value()],
            };
            if (negate_src2) {
                src2[0] = -src2[0];
                src2[1] = -src2[1];
                src2[2] = -src2[2];
                src2[3] = -src2[3];
            }

            float24* dest =
                (instr.common.dest.Value() < 0x10)
                    ? &state.registers.output[instr.common.dest.Value().GetIndex()][0]
                    : (instr.common.dest.Value() < 0x20)
                          ? &state.registers.temporary[instr.common.dest.Value().GetIndex()][0]
                          : dummy_vec4_float24;

            switch (instr.opcode.Value().EffectiveOpCode()) {
            case OpCode::Id::ADD: {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] + src2[i];
                }
                break;
            }

            case OpCode::Id::MUL: {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = src1[i] * src2[i];
                }
                break;
            }

            case OpCode::Id::FLR:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }
                    dest[i] = float24::FromFloat32(std::floor(src1[i].ToFloat32()));
                }
                break;

            case OpCode::Id::MAX:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    // NOTE: Exact form required to match NaN semantics to hardware:
                    //   max(0, NaN) -> NaN
                    //   max(NaN, 0) -> 0
                    dest[i] = (src1[i] > src2[i]) ? src1[i] : src2[i];
                }
                break;

            case OpCode::Id::MIN:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    // NOTE: Exact form required to match NaN semantics to hardware:
                    //   min(0, NaN) -> NaN
                    //   min(NaN, 0) -> 0
                    dest[i] = (src1[i] < src2[i]) ? src1[i] : src2[i];
                }
                break;

            case OpCode::Id::DP3:
            case OpCode::Id::DP4:
            case OpCode::Id::DPH:
            case OpCode::Id::DPHI: {
                OpCode::Id opcode = instr.opcode.Value().EffectiveOpCode();
                if (opcode == OpCode::Id::DPH || opcode == OpCode::Id::DPHI) {
                    src1[3] = float24::FromFloat32(1.0f);
                }

                int num_components = (opcode == OpCode::Id::DP3) ? 3 : 4;
                float24 dot = std::inner_product(src1, src1 + num_components, src2,
                                                 float24::FromFloat32(0.f));

                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    dest[i] = dot;
                }
                break;
            }

            // Reciprocal
            case OpCode::Id::RCP: {
                float24 rcp_res = float24::FromFloat32(1.0f / src1[0].ToFloat32());
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = rcp_res;
                }
                break;
            }

            // Reciprocal Square Root
            case OpCode::Id::RSQ: {
                float24 rsq_res = float24::FromFloat32(1.0f / std::sqrt(src1[0].ToFloat32()));
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i))
                        continue;

                    dest[i] = rsq_res;
                }
                break;
            }

            case OpCode::Id::MOVA: {
                for (int i = 0; i < 2; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    // TODO: Figure out how the rounding is done on hardware
                    state.address_registers[i] = static_cast<s32>(src1[i].ToFloat32());
                }
                break;
            }

            case OpCode::Id::MOV: {
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    dest[i] = src1[i];
                }
                break;
            }

            case OpCode::Id::SGE:
            case OpCode::Id::SGEI:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    dest[i] = (src1[i] >= src2[i]) ? float24::FromFloat32(1.0f)
                                                   : float24::FromFloat32(0.0f);
                }
                break;

            case OpCode::Id::SLT:
            case OpCode::Id::SLTI:
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    dest[i] = (src1[i] < src2[i]) ? float24::FromFloat32(1.0f)
                                                  : float24::FromFloat32(0.0f);
                }
                break;

            case OpCode::Id::CMP:
                for (int i = 0; i < 2; ++i) {
                    // TODO: Can you restrict to one compare via dest masking?

                    auto compare_op = instr.common.compare_op;
                    auto op = (i == 0) ? compare_op.x.Value() : compare_op.y.Value();

                    switch (op) {
                    case Instruction::Common::CompareOpType::Equal:
                        state.conditional_code[i] = (src1[i] == src2[i]);
                        break;

                    case Instruction::Common::CompareOpType::NotEqual:
                        state.conditional_code[i] = (src1[i] != src2[i]);
                        break;

                    case Instruction::Common::CompareOpType::LessThan:
                        state.conditional_code[i] = (src1[i] < src2[i]);
                        break;

                    case Instruction::Common::CompareOpType::LessEqual:
                        state.conditional_code[i] = (src1[i] <= src2[i]);
                        break;

                    case Instruction::Common::CompareOpType::GreaterThan:
                        state.conditional_code[i] = (src1[i] > src2[i]);
                        break;

                    case Instruction::Common::CompareOpType::GreaterEqual:
                        state.conditional_code[i] = (src1[i] >= src2[i]);
                        break;

                    default:
                        LOG_ERROR(HW_GPU, "Unknown compare mode {:x}", static_cast<int>(op));
                        break;
                    }
                }
                break;

            case OpCode::Id::EX2: {
                // EX2 only takes first component exp2 and writes it to all dest components
                float24 ex2_res = float24::FromFloat32(std::exp2(src1[0].ToFloat32()));
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    dest[i] = ex2_res;
                }

                break;
            }

            case OpCode::Id::LG2: {
                // LG2 only takes the first component log2 and writes it to all dest components
                float24 lg2_res = float24::FromFloat32(std::log2(src1[0].ToFloat32()));
                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    dest[i] = lg2_res;
                }

                break;
            }

            default:
                LOG_ERROR(HW_GPU, "Unhandled arithmetic instruction: 0x{:02x} ({}): 0x{:08x}",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
                DEBUG_ASSERT(false);
                break;
            }

            break;
        }

        case OpCode::Type::MultiplyAdd: {
            if ((instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MAD) ||
                (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI)) {
                const SwizzlePattern& swizzle = *reinterpret_cast<const SwizzlePattern*>(
                    &swizzle_data[instr.mad.operand_desc_id]);

                bool is_inverted = (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI);

                const int address_offset =
                    (instr.mad.address_register_index == 0)
                        ? 0
                        : state.address_registers[instr.mad.address_register_index - 1];

                const float24* src1_ = LookupSourceRegister(instr.mad.GetSrc1(is_inverted));
                const float24* src2_ = LookupSourceRegister(instr.mad.GetSrc2(is_inverted) +
                                                            (!is_inverted * address_offset));
                const float24* src3_ = LookupSourceRegister(instr.mad.GetSrc3(is_inverted) +
                                                            (is_inverted * address_offset));

                const bool negate_src1 = ((bool)swizzle.negate_src1 != false);
                const bool negate_src2 = ((bool)swizzle.negate_src2 != false);
                const bool negate_src3 = ((bool)swizzle.negate_src3 != false);

                float24 src1[4] = {
                    src1_[(int)swizzle.src1_selector_0.Value()],
                    src1_[(int)swizzle.src1_selector_1.Value()],
                    src1_[(int)swizzle.src1_selector_2.Value()],
                    src1_[(int)swizzle.src1_selector_3.Value()],
                };
                if (negate_src1) {
                    src1[0] = -src1[0];
                    src1[1] = -src1[1];
                    src1[2] = -src1[2];
                    src1[3] = -src1[3];
                }
                float24 src2[4] = {
                    src2_[(int)swizzle.src2_selector_0.Value()],
                    src2_[(int)swizzle.src2_selector_1.Value()],
                    src2_[(int)swizzle.src2_selector_2.Value()],
                    src2_[(int)swizzle.src2_selector_3.Value()],
                };
                if (negate_src2) {
                    src2[0] = -src2[0];
                    src2[1] = -src2[1];
                    src2[2] = -src2[2];
                    src2[3] = -src2[3];
                }
                float24 src3[4] = {
                    src3_[(int)swizzle.src3_selector_0.Value()],
                    src3_[(int)swizzle.src3_selector_1.Value()],
                    src3_[(int)swizzle.src3_selector_2.Value()],
                    src3_[(int)swizzle.src3_selector_3.Value()],
                };
                if (negate_src3) {
                    src3[0] = -src3[0];
                    src3[1] = -src3[1];
                    src3[2] = -src3[2];
                    src3[3] = -src3[3];
                }

                float24* dest =
                    (instr.mad.dest.Value() < 0x10)
                        ? &state.registers.output[instr.mad.dest.Value().GetIndex()][0]
                        : (instr.mad.dest.Value() < 0x20)
                              ? &state.registers.temporary[instr.mad.dest.Value().GetIndex()][0]
                              : dummy_vec4_float24;

                for (int i = 0; i < 4; ++i) {
                    if (!swizzle.DestComponentEnabled(i)) {
                        continue;
                    }

                    dest[i] = src1[i] * src2[i] + src3[i];
                }
            } else {
                LOG_ERROR(HW_GPU, "Unhandled multiply-add instruction: 0x{:02x} ({}): 0x{:08x}",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
            }
            break;
        }

        default: {
            // Handle each instruction on its own
            switch (instr.opcode.Value()) {
            case OpCode::Id::END:
                exit_loop = true;
                break;

            case OpCode::Id::JMPC:
                if (evaluate_condition(instr.flow_control)) {
                    program_counter = instr.flow_control.dest_offset - 1;
                }
                break;

            case OpCode::Id::JMPU:
                if (uniforms.b[instr.flow_control.bool_uniform_id] ==
                    !(instr.flow_control.num_instructions & 1)) {
                    program_counter = instr.flow_control.dest_offset - 1;
                }
                break;

            case OpCode::Id::CALL:
                call(instr.flow_control.dest_offset, instr.flow_control.num_instructions,
                     program_counter + 1, 0, 0);
                break;

            case OpCode::Id::CALLU:
                if (uniforms.b[instr.flow_control.bool_uniform_id]) {
                    call(instr.flow_control.dest_offset, instr.flow_control.num_instructions,
                         program_counter + 1, 0, 0);
                }
                break;

            case OpCode::Id::CALLC:
                if (evaluate_condition(instr.flow_control)) {
                    call(instr.flow_control.dest_offset, instr.flow_control.num_instructions,
                         program_counter + 1, 0, 0);
                }
                break;

            case OpCode::Id::NOP:
                break;

            case OpCode::Id::IFU:
                if (uniforms.b[instr.flow_control.bool_uniform_id]) {
                    call(program_counter + 1, instr.flow_control.dest_offset - program_counter - 1,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0,
                         0);
                } else {
                    call(instr.flow_control.dest_offset, instr.flow_control.num_instructions,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0,
                         0);
                }

                break;

            case OpCode::Id::IFC: {
                // TODO: Do we need to consider swizzlers here?
                if (evaluate_condition(instr.flow_control)) {
                    call(program_counter + 1, instr.flow_control.dest_offset - program_counter - 1,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0,
                         0);
                } else {
                    call(instr.flow_control.dest_offset, instr.flow_control.num_instructions,
                         instr.flow_control.dest_offset + instr.flow_control.num_instructions, 0,
                         0);
                }
                break;
            }

            case OpCode::Id::LOOP: {
                Common::Vec4<u8> loop_param(uniforms.i[instr.flow_control.int_uniform_id].x,
                                            uniforms.i[instr.flow_control.int_uniform_id].y,
                                            uniforms.i[instr.flow_control.int_uniform_id].z,
                                            uniforms.i[instr.flow_control.int_uniform_id].w);
                state.address_registers[2] = loop_param.y;

                call(program_counter + 1, instr.flow_control.dest_offset - program_counter,
                     instr.flow_control.dest_offset + 1, loop_param.x, loop_param.z);

                break;
            }

            case OpCode::Id::EMIT: {
                GSEmitter* emitter = state.emitter_ptr;
                ASSERT_MSG(emitter, "Execute EMIT on VS");
                emitter->Emit(state.registers.output);
                break;
            }

            case OpCode::Id::SETEMIT: {
                GSEmitter* emitter = state.emitter_ptr;
                ASSERT_MSG(emitter, "Execute SETEMIT on VS");
                emitter->vertex_id = instr.setemit.vertex_id;
                emitter->prim_emit = instr.setemit.prim_emit != 0;
                emitter->winding = instr.setemit.winding != 0;
                break;
            }

            default:
                LOG_ERROR(HW_GPU, "Unhandled instruction: 0x{:02x} ({}): 0x{:08x}",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
                break;
            }

            break;
        }
        }

        ++program_counter;
        ++iteration;
    }
}

void InterpreterEngine::SetupBatch(ShaderSetup& setup, unsigned int entry_point) {
    ASSERT(entry_point < MAX_PROGRAM_CODE_LENGTH);
    setup.engine_data.entry_point = entry_point;
}

void InterpreterEngine::Run(const ShaderSetup& setup, UnitState& state) const {
    RunInterpreter(setup, state, setup.engine_data.entry_point);
}

} // namespace Pica::Shader
