#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

static const uint32_t MEMORY_SIZE = 0x100000; // 1 MB de memoria plana
static const uint32_t LOAD_ADDR   = 0x00000000;

struct Simulator {
    uint32_t pc;
    uint32_t regs[32];
    uint8_t  mem[MEMORY_SIZE];
    bool     running;
    int      exit_code;

    Simulator() : pc(LOAD_ADDR), running(false), exit_code(0) {
        memset(regs, 0, sizeof(regs));
        memset(mem,  0, sizeof(mem));
    }
};


bool addr_valid(uint32_t addr, uint32_t size) {
    return (uint64_t)addr + size <= MEMORY_SIZE;
}

uint8_t mem_read_byte(Simulator& sim, uint32_t addr) {
    if (!addr_valid(addr, 1)) {
        std::cerr << "[ERROR] Lectura fuera de rango: 0x"
                  << std::hex << addr << "\n";
        sim.running = false;
        return 0;
    }
    return sim.mem[addr];
}

uint16_t mem_read_half(Simulator& sim, uint32_t addr) {
    if (!addr_valid(addr, 2)) {
        std::cerr << "[ERROR] Lectura fuera de rango: 0x"
                  << std::hex << addr << "\n";
        sim.running = false;
        return 0;
    }
    return (uint16_t)sim.mem[addr] |
           ((uint16_t)sim.mem[addr + 1] << 8);
}

uint32_t mem_read_word(Simulator& sim, uint32_t addr) {
    if (!addr_valid(addr, 4)) {
        std::cerr << "[ERROR] Lectura fuera de rango: 0x"
                  << std::hex << addr << "\n";
        sim.running = false;
        return 0;
    }
    return (uint32_t)sim.mem[addr]           |
           ((uint32_t)sim.mem[addr + 1] << 8)  |
           ((uint32_t)sim.mem[addr + 2] << 16) |
           ((uint32_t)sim.mem[addr + 3] << 24);
}

void mem_write_byte(Simulator& sim, uint32_t addr, uint8_t val) {
    if (!addr_valid(addr, 1)) {
        std::cerr << "[ERROR] Escritura fuera de rango: 0x"
                  << std::hex << addr << "\n";
        sim.running = false;
        return;
    }
    sim.mem[addr] = val;
}

void mem_write_half(Simulator& sim, uint32_t addr, uint16_t val) {
    if (!addr_valid(addr, 2)) {
        std::cerr << "[ERROR] Escritura fuera de rango: 0x"
                  << std::hex << addr << "\n";
        sim.running = false;
        return;
    }
    sim.mem[addr]     = val & 0xFF;
    sim.mem[addr + 1] = (val >> 8) & 0xFF;
}

void mem_write_word(Simulator& sim, uint32_t addr, uint32_t val) {
    if (!addr_valid(addr, 4)) {
        std::cerr << "[ERROR] Escritura fuera de rango: 0x"
                  << std::hex << addr << "\n";
        sim.running = false;
        return;
    }
    sim.mem[addr]     = val & 0xFF;
    sim.mem[addr + 1] = (val >> 8)  & 0xFF;
    sim.mem[addr + 2] = (val >> 16) & 0xFF;
    sim.mem[addr + 3] = (val >> 24) & 0xFF;
}

bool load_binary(Simulator& sim, const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) {
        std::cerr << "[ERROR] No se pudo abrir: " << path << "\n";
        return false;
    }

    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    if ((uint64_t)size > MEMORY_SIZE - LOAD_ADDR) {
        std::cerr << "[ERROR] El programa no cabe en memoria ("
                  << size << " bytes).\n";
        return false;
    }

    f.read(reinterpret_cast<char*>(sim.mem + LOAD_ADDR), size);
    if (!f) {
        std::cerr << "[ERROR] Fallo al leer el archivo.\n";
        return false;
    }

    std::cout << "\"" << path << "\" cargado a memoria ("
              << std::dec << size << " bytes).\n";
    return true;
}

void execute(Simulator& sim);
std::string disassemble(uint32_t instr);
extern bool g_verbose_print;


static const char* REG_NAMES[32] = {
    "x0","x1","x2","x3","x4","x5","x6","x7",
    "x8","x9","x10","x11","x12","x13","x14","x15",
    "x16","x17","x18","x19","x20","x21","x22","x23",
    "x24","x25","x26","x27","x28","x29","x30","x31"
};

int reg_index(const std::string& name) {
    for (int i = 0; i < 32; i++) {
        if (name == REG_NAMES[i]) return i;
    }
    // También acepta "zero","ra","sp","gp","tp","t0-t6","s0-s11","a0-a7"
    static const char* ABI[32] = {
        "zero","ra","sp","gp","tp","t0","t1","t2",
        "s0","s1","a0","a1","a2","a3","a4","a5",
        "a6","a7","s2","s3","s4","s5","s6","s7",
        "s8","s9","s10","s11","t3","t4","t5","t6"
    };
    for (int i = 0; i < 32; i++) {
        if (name == ABI[i]) return i;
    }
    return -1;
}

void cmd_pc(Simulator& sim) {
    std::cout << "pc = 0x"
              << std::hex << std::setw(8) << std::setfill('0')
              << sim.pc << "\n";
}

void cmd_regs(Simulator& sim, const std::vector<std::string>& args) {
    if (args.empty()) {
        for (int i = 0; i < 32; i++) {
            std::cout << std::left << std::setw(5) << REG_NAMES[i]
                      << " = 0x"
                      << std::hex << std::setw(8) << std::setfill('0')
                      << sim.regs[i] << "\n";
        }
    } else {
        for (const auto& name : args) {
            int idx = reg_index(name);
            if (idx < 0) {
                std::cout << "[ERROR] Registro desconocido: " << name << "\n";
            } else {
                std::cout << REG_NAMES[idx] << " = 0x"
                          << std::hex << std::setw(8) << std::setfill('0')
                          << sim.regs[idx] << "\n";
            }
        }
    }
}

void cmd_mem(Simulator& sim, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Uso: mem <start_hex> <end_hex>\n";
        return;
    }
    uint32_t start = (uint32_t)std::stoul(args[0], nullptr, 16);
    uint32_t end   = (uint32_t)std::stoul(args[1], nullptr, 16);

    if (start > end || !addr_valid(start, 1) || !addr_valid(end, 1)) {
        std::cout << "[ERROR] Rango inválido.\n";
        return;
    }

    std::cout << "Memoria (0x" << std::hex << std::setw(4) << std::setfill('0') << start
              << "-0x" << std::setw(4) << end << "):";
    for (uint32_t a = start; a <= end; a++) {
        std::cout << " " << std::hex << std::setw(2) << std::setfill('0')
                  << (unsigned)sim.mem[a];
    }
    std::cout << "\n";
}

void cmd_disasm(Simulator& sim, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cout << "Uso: disasm <start_hex> <end_hex>\n";
        return;
    }
    uint32_t start = (uint32_t)std::stoul(args[0], nullptr, 16);
    uint32_t end   = (uint32_t)std::stoul(args[1], nullptr, 16);

    if (start > end || !addr_valid(start, 4) || !addr_valid(end, 4)) {
        std::cout << "[ERROR] Rango inválido.\n";
        return;
    }

    for (uint32_t a = start; a <= end; a += 4) {
        uint32_t instr = mem_read_word(sim, a);
        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << a
                  << ":  " << disassemble(instr) << "\n" << std::dec;
    }
}

void cmd_step(Simulator& sim) {
    if (!sim.running) {
        std::cout << "[INFO] El simulador no está corriendo. Usa 'run' para iniciar.\n";
        return;
    }
    execute(sim);
}

void cmd_run(Simulator& sim) {
    sim.running = true;
    std::cout << "Ejecutando programa...\n";

    bool prev_verbose = g_verbose_print;
    g_verbose_print = false; // 'run' no imprime cada instrucción (evita inundar la terminal)

    uint32_t prev_pc = sim.pc;
    uint64_t instr_count = 0;
    const uint64_t MAX_INSTR = 5'000'000; 

    while (sim.running) {
        prev_pc = sim.pc;
        execute(sim);
        instr_count++;

        // Detecta salto infinito a sí mismo: j . / j _end (jal/jalr/beq con target == su propio PC)
        if (sim.running && sim.pc == prev_pc) {
            std::cout << "[INFO] Loop infinito detectado en 0x"
                      << std::hex << std::setw(8) << std::setfill('0') << sim.pc
                      << std::dec << " (instrucción salta a sí misma). Deteniendo.\n";
            sim.running = false;
            break;
        }

        if (instr_count >= MAX_INSTR) {
            std::cout << "[WARN] Límite de " << MAX_INSTR
                      << " instrucciones alcanzado. Deteniendo por seguridad.\n";
            sim.running = false;
            break;
        }
    }

    g_verbose_print = prev_verbose;
    std::cout << "Total de instrucciones ejecutadas: " << std::dec << instr_count << "\n";
    std::cout << "Programa terminado con código " << std::dec << sim.exit_code << ".\n";
}

void cmd_help() {
    std::cout
        << "Comandos disponibles:\n"
        << "  step              - Ejecuta una instrucción\n"
        << "  run               - Ejecuta hasta terminar\n"
        << "  pc                - Muestra el PC\n"
        << "  regs [x0 x1 ...]  - Muestra registros (todos si no se especifica)\n"
        << "  mem <start> <end> - Muestra memoria en rango hex\n"
        << "  disasm <s> <e>    - Desensambla instrucciones en rango hex\n"
        << "  reset             - Reinicia el simulador\n"
        << "  exit              - Sale del simulador\n";
}

void repl(Simulator& sim) {
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;

        std::vector<std::string> args;
        std::string tok;
        while (ss >> tok) args.push_back(tok);

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "See you next time...\n";
            std::cout << "Program exited with code " << std::dec << sim.exit_code << ".\n";
            break;
        } else if (cmd == "pc") {
            cmd_pc(sim);
        } else if (cmd == "regs") {
            cmd_regs(sim, args);
        } else if (cmd == "mem") {
            cmd_mem(sim, args);
        } else if (cmd == "disasm") {
            cmd_disasm(sim, args);
        } else if (cmd == "step") {
            sim.running = true;
            cmd_step(sim);
        } else if (cmd == "run") {
            cmd_run(sim);
        } else if (cmd == "reset") {
            sim.pc       = LOAD_ADDR;
            sim.running  = false;
            sim.exit_code = 0;
            memset(sim.regs, 0, sizeof(sim.regs));
            std::cout << "Simulador reiniciado (memoria intacta).\n";
        } else if (cmd == "help") {
            cmd_help();
        } else if (cmd.empty()) {
            // línea vacía, ignorar
        } else {
            std::cout << "Comando desconocido: \"" << cmd
                      << "\". Escribe 'help' para ver opciones.\n";
        }
    }
}

int main(int argc, char* argv[]) {
    Simulator sim;

    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <programa.bin>\n";
        return 1;
    }

    if (!load_binary(sim, argv[1])) return 1;

    sim.running = true;
    cmd_help();
    std::cout << "\n";

    repl(sim);
    return 0;
}


static inline uint32_t bits(uint32_t instr, int hi, int lo) {
    return (instr >> lo) & ((1u << (hi - lo + 1)) - 1);
}

// Sign-extend un valor de 'n' bits a 32 bits con signo
static inline int32_t sign_ext(uint32_t val, int n) {
    uint32_t mask = 1u << (n - 1);
    return (int32_t)((val ^ mask) - mask);
}

struct Decoded {
    uint32_t opcode;
    uint32_t rd, rs1, rs2;
    uint32_t funct3, funct7;
    int32_t  imm;
};

Decoded decode(uint32_t instr) {
    Decoded d;
    d.opcode = bits(instr, 6, 0);
    d.rd     = bits(instr, 11, 7);
    d.funct3 = bits(instr, 14, 12);
    d.rs1    = bits(instr, 19, 15);
    d.rs2    = bits(instr, 24, 20);
    d.funct7 = bits(instr, 31, 25);

    // Inmediato tipo I
    int32_t imm_i = sign_ext(bits(instr, 31, 20), 12);

    // Inmediato tipo S
    uint32_t s_hi  = bits(instr, 31, 25);
    uint32_t s_lo  = bits(instr, 11, 7);
    int32_t  imm_s = sign_ext((s_hi << 5) | s_lo, 12);

    // Inmediato tipo B
    uint32_t b12   = bits(instr, 31, 31);
    uint32_t b11   = bits(instr, 7,  7);
    uint32_t b10_5 = bits(instr, 30, 25);
    uint32_t b4_1  = bits(instr, 11, 8);
    int32_t  imm_b = sign_ext((b12 << 12) | (b11 << 11) | (b10_5 << 5) | (b4_1 << 1), 13);

    // Inmediato tipo U
    int32_t imm_u = (int32_t)(instr & 0xFFFFF000u);

    // Inmediato tipo J
    uint32_t j20    = bits(instr, 31, 31);
    uint32_t j19_12 = bits(instr, 19, 12);
    uint32_t j11    = bits(instr, 20, 20);
    uint32_t j10_1  = bits(instr, 30, 21);
    int32_t  imm_j  = sign_ext((j20 << 20) | (j19_12 << 12) | (j11 << 11) | (j10_1 << 1), 21);

    // Asignar inmediato según opcode
    switch (d.opcode) {
        case 0x03: // loads  (I)
        case 0x13: // op-imm (I)
        case 0x67: // jalr   (I)
        case 0x73: // ecall  (I)
            d.imm = imm_i; break;
        case 0x23: // stores (S)
            d.imm = imm_s; break;
        case 0x63: // branch (B)
            d.imm = imm_b; break;
        case 0x37: // lui    (U)
        case 0x17: // auipc  (U)
            d.imm = imm_u; break;
        case 0x6F: // jal    (J)
            d.imm = imm_j; break;
        default:
            d.imm = 0;
    }

    return d;
}

static const char* abi_name(uint32_t reg) {
    static const char* ABI[32] = {
        "zero","ra","sp","gp","tp","t0","t1","t2",
        "s0","s1","a0","a1","a2","a3","a4","a5",
        "a6","a7","s2","s3","s4","s5","s6","s7",
        "s8","s9","s10","s11","t3","t4","t5","t6"
    };
    return ABI[reg & 0x1F];
}

std::string disassemble(uint32_t instr) {
    if (instr == 0x00000000) return "(nulo)";

    Decoded d = decode(instr);
    std::ostringstream out;

    auto reg = [](uint32_t r) { return std::string(abi_name(r)); };

    switch (d.opcode) {
    case 0x03: { // loads
        const char* names[] = {"lb","lh","lw",nullptr,"lbu","lhu"};
        const char* n = (d.funct3 < 6 && names[d.funct3]) ? names[d.funct3] : "l??";
        out << n << " " << reg(d.rd) << ", " << d.imm << "(" << reg(d.rs1) << ")";
        break;
    }
    case 0x13: { // op-imm
        switch (d.funct3) {
            case 0x0: out << "addi " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.imm; break;
            case 0x1: out << "slli " << reg(d.rd) << ", " << reg(d.rs1) << ", " << (d.imm & 0x1F); break;
            case 0x2: out << "slti " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.imm; break;
            case 0x3: out << "sltiu " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.imm; break;
            case 0x4: out << "xori " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.imm; break;
            case 0x5:
                if (d.funct7 == 0x00) out << "srli " << reg(d.rd) << ", " << reg(d.rs1) << ", " << (d.imm & 0x1F);
                else                  out << "srai " << reg(d.rd) << ", " << reg(d.rs1) << ", " << (d.imm & 0x1F);
                break;
            case 0x6: out << "ori " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.imm; break;
            case 0x7: out << "andi " << reg(d.rd) << ", " << reg(d.rs1) << ", " << d.imm; break;
        }
        break;
    }
    case 0x17: out << "auipc " << reg(d.rd) << ", " << ((uint32_t)d.imm >> 12); break;
    case 0x23: { // stores
        const char* names[] = {"sb","sh","sw"};
        const char* n = (d.funct3 < 3) ? names[d.funct3] : "s??";
        out << n << " " << reg(d.rs2) << ", " << d.imm << "(" << reg(d.rs1) << ")";
        break;
    }
    case 0x33: { // op (R)
        switch (d.funct3) {
            case 0x0: out << (d.funct7 == 0x00 ? "add " : "sub ") << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
            case 0x1: out << "sll " << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
            case 0x2: out << "slt " << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
            case 0x3: out << "sltu " << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
            case 0x4: out << "xor " << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
            case 0x5: out << (d.funct7 == 0x00 ? "srl " : "sra ") << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
            case 0x6: out << "or " << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
            case 0x7: out << "and " << reg(d.rd) << ", " << reg(d.rs1) << ", " << reg(d.rs2); break;
        }
        break;
    }
    case 0x37: out << "lui " << reg(d.rd) << ", " << ((uint32_t)d.imm >> 12); break;
    case 0x63: { // branch
        const char* names[] = {"beq","bne",nullptr,nullptr,"blt","bge","bltu","bgeu"};
        const char* n = (d.funct3 < 8 && names[d.funct3]) ? names[d.funct3] : "b??";
        out << n << " " << reg(d.rs1) << ", " << reg(d.rs2) << ", " << d.imm;
        break;
    }
    case 0x67: out << "jalr " << reg(d.rd) << ", " << d.imm << "(" << reg(d.rs1) << ")"; break;
    case 0x6F: out << "jal " << reg(d.rd) << ", " << d.imm; break;
    case 0x73: out << "ecall"; break;
    default:   out << "??? (opcode 0x" << std::hex << d.opcode << ")";
    }

    return out.str();
}

//Execute 

bool g_verbose_print = true; // controla si execute() imprime cada instrucción

void execute(Simulator& sim) {
    if (!addr_valid(sim.pc, 4)) {
        std::cerr << "[ERROR] PC fuera de rango: 0x"
                  << std::hex << sim.pc << "\n";
        sim.running = false;
        return;
    }

    uint32_t instr = mem_read_word(sim, sim.pc);

    // Instrucción nula → detener

    if (instr == 0x00000000) {
        sim.running = false;
        return;
    }

    Decoded d = decode(instr);
    uint32_t next_pc = sim.pc + 4;

    if (g_verbose_print) {
        std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << sim.pc
                  << ":  " << disassemble(instr) << "\n" << std::dec;
    }

    // x0 siempre es 0 
    sim.regs[0] = 0;

    int32_t  rs1s = (int32_t)sim.regs[d.rs1];
    int32_t  rs2s = (int32_t)sim.regs[d.rs2];
    uint32_t rs1u = sim.regs[d.rs1];
    uint32_t rs2u = sim.regs[d.rs2];

    switch (d.opcode) {

    // Loads (I)
    case 0x03: {
        uint32_t addr = (uint32_t)(rs1s + d.imm);
        uint32_t val  = 0;
        switch (d.funct3) {
            case 0x0: val = (uint32_t)sign_ext(mem_read_byte(sim, addr), 8);  break; // lb
            case 0x1: val = (uint32_t)sign_ext(mem_read_half(sim, addr), 16); break; // lh
            case 0x2: val = mem_read_word(sim, addr);                          break; // lw
            case 0x4: val = mem_read_byte(sim, addr);                          break; // lbu
            case 0x5: val = mem_read_half(sim, addr);                          break; // lhu
            default:
                std::cerr << "[ERROR] load funct3 desconocido: " << d.funct3 << "\n";
                sim.running = false; return;
        }
        if (d.rd != 0) sim.regs[d.rd] = val;
        break;
    }

    //Op-Imm (I) 
    case 0x13: {
        uint32_t shamt = (uint32_t)d.imm & 0x1F;
        uint32_t result = 0;
        switch (d.funct3) {
            case 0x0: result = (uint32_t)(rs1s + d.imm);              break; // addi
            case 0x1: result = rs1u << shamt;                          break; // slli
            case 0x2: result = (rs1s < d.imm) ? 1 : 0;               break; // slti
            case 0x3: result = (rs1u < (uint32_t)d.imm) ? 1 : 0;    break; // sltiu
            case 0x4: result = rs1u ^ (uint32_t)d.imm;               break; // xori
            case 0x5:
                if (d.funct7 == 0x00) result = rs1u >> shamt;          // srli
                else                  result = (uint32_t)(rs1s >> (int)shamt); // srai
                break;
            case 0x6: result = rs1u | (uint32_t)d.imm;               break; // ori
            case 0x7: result = rs1u & (uint32_t)d.imm;               break; // andi
            default:
                std::cerr << "[ERROR] op-imm funct3 desconocido: " << d.funct3 << "\n";
                sim.running = false; return;
        }
        if (d.rd != 0) sim.regs[d.rd] = result;
        break;
    }

    //U
    case 0x17:
        if (d.rd != 0) sim.regs[d.rd] = sim.pc + (uint32_t)d.imm;
        break;

    // Stores (S)
    case 0x23: {
        uint32_t addr = (uint32_t)(rs1s + d.imm);
        switch (d.funct3) {
            case 0x0: mem_write_byte(sim, addr, (uint8_t)rs2u);         break; // sb
            case 0x1: mem_write_half(sim, addr, (uint16_t)rs2u);        break; // sh
            case 0x2: mem_write_word(sim, addr, rs2u);                   break; // sw
            default:
                std::cerr << "[ERROR] store funct3 desconocido: " << d.funct3 << "\n";
                sim.running = false; return;
        }
        break;
    }

    // Op (R)
    case 0x33: {
        uint32_t shamt = rs2u & 0x1F;
        uint32_t result = 0;
        switch (d.funct3) {
            case 0x0:
                if (d.funct7 == 0x00) result = (uint32_t)(rs1s + rs2s);  // add
                else                  result = (uint32_t)(rs1s - rs2s);  // sub
                break;
            case 0x1: result = rs1u << shamt;                             break; // sll
            case 0x2: result = (rs1s < rs2s) ? 1 : 0;                   break; // slt
            case 0x3: result = (rs1u < rs2u) ? 1 : 0;                   break; // sltu
            case 0x4: result = rs1u ^ rs2u;                               break; // xor
            case 0x5:
                if (d.funct7 == 0x00) result = rs1u >> shamt;             // srl
                else                  result = (uint32_t)(rs1s >> (int)shamt); // sra
                break;
            case 0x6: result = rs1u | rs2u;                               break; // or
            case 0x7: result = rs1u & rs2u;                               break; // and
            default:
                std::cerr << "[ERROR] op-R funct3 desconocido: " << d.funct3 << "\n";
                sim.running = false; return;
        }
        if (d.rd != 0) sim.regs[d.rd] = result;
        break;
    }

    // LUI (U)
    case 0x37:
        if (d.rd != 0) sim.regs[d.rd] = (uint32_t)d.imm;
        break;

    // Branch (B) 
    case 0x63: {
        bool taken = false;
        switch (d.funct3) {
            case 0x0: taken = (rs1s == rs2s);  break; // beq
            case 0x1: taken = (rs1s != rs2s);  break; // bne
            case 0x4: taken = (rs1s <  rs2s);  break; // blt
            case 0x5: taken = (rs1s >= rs2s);  break; // bge
            case 0x6: taken = (rs1u <  rs2u);  break; // bltu
            case 0x7: taken = (rs1u >= rs2u);  break; // bgeu
            default:
                std::cerr << "[ERROR] branch funct3 desconocido: " << d.funct3 << "\n";
                sim.running = false; return;
        }
        if (taken) next_pc = (uint32_t)((int32_t)sim.pc + d.imm);
        break;
    }

    // JALR (I) 
    case 0x67: {
        uint32_t target = (uint32_t)(rs1s + d.imm) & ~1u;
        if (d.rd != 0) sim.regs[d.rd] = next_pc;
        next_pc = target;
        break;
    }

    // JAL (J) 
    case 0x6F:
        if (d.rd != 0) sim.regs[d.rd] = next_pc;
        next_pc = (uint32_t)((int32_t)sim.pc + d.imm);
        break;

    // ECALL 
    case 0x73: {
        uint32_t syscall = sim.regs[17]; // a7
        switch (syscall) {
            case 1:  // print_int
                std::cout << (int32_t)sim.regs[10];
                break;
            case 4:  // print_string
            {
                uint32_t ptr = sim.regs[10];
                while (addr_valid(ptr, 1) && sim.mem[ptr] != 0)
                    std::cout << (char)sim.mem[ptr++];
                break;
            }
            case 10: // exit
                sim.exit_code = 0;
                sim.running = false;
                return;
            case 11: // print_char
                std::cout << (char)sim.regs[10];
                break;
            case 17: // exit2
                sim.exit_code = (int)sim.regs[10];
                sim.running = false;
                return;
            default:
                std::cerr << "[WARN] ecall " << syscall << " no implementado.\n";
        }
        break;
    }

    default:
        std::cerr << "[ERROR] Opcode desconocido: 0x"
                  << std::hex << d.opcode
                  << " en PC=0x" << sim.pc << "\n";
        sim.running = false;
        return;
    }

    // x0 siempre 0 (por si alguna instrucción lo escribió)
    sim.regs[0] = 0;
    sim.pc = next_pc;
}
