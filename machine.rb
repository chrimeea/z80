# frozen_string_literal: true

require 'tk'

module Z80
    MAX0 = 0x01
    MAX1 = 0x02
    MAX2 = 0x04
    MAX3 = 0x08
    MAX4 = 0x10
    MAX5 = 0x20
    MAX6 = 0x40
    MAX7 = 0x80
    MAX8 = 0x100
    MAX15 = 0x8000
    MAX16 = 0x10000
    MAX = [MAX0, MAX1, MAX2, MAX3, MAX4, MAX5, MAX6, MAX7]

    class GeneralRegister
        def initialize n
            @size = n
        end

        def to_s base = 16
            if base == 2
                "%0#{@size}b" % self.byte_value
            else
                "%0#{@size / 4}X" % self.byte_value
            end
        end

        def bit?(b)
            fail if b < 0 || b >= @size
            self.to_s(2).reverse[b] == '1'
        end

        def max_byte_value(n = @size)
            2 ** n
        end

        def two_complement
            max = max_byte_value
            bv = self.byte_value
            bv >= max / 2 ? bv - max : bv
        end

        def store(num)
            max = max_byte_value
            if num >= max / 2 || num < -(max / 2)
                num = (max - 1) & num
                @overflow = true
            else
                @overflow = false
            end
            self.store_byte_value(num.negative? ? num + max : num)
        end
    end

    class Register8 < GeneralRegister
        attr_reader :byte_value, :overflow, :hc, :carry, :n

        def initialize
            @byte_value = 0
            @overflow, @hc, @carry, @n = false, false, false, false
            super(8)
        end

        def negative?
            @byte_value >= MAX7
        end

        def to_4_bit_pair
            @byte_value.divmod MAX4
        end

        def store_4_bit_pair(high4, low4)
            fail if high4 < 0 || high4 >= MAX4 || low4 < 0 || low4 >= MAX4
            @byte_value = high4 * MAX4 + low4
        end

        def set_bit(b, value = true)
            fail if b < 0 || b > 7
            if value
                @byte_value |= MAX[b]
            else
                @byte_value &= ~(MAX[b] + MAX8)
            end
        end

        def reset_bit(b)
            set_bit(b, false)
        end

        def negate
            @byte_value = ~(@byte_value + MAX8)
            @n = true
        end

        def shift_left
            if self.negative?
                @carry = true
                @byte_value = @byte_value << 1
            else
                @carry = false
                @byte_value = @byte_value << 1
            end
            @n, @hc = false, false
        end

        def shift_right
            @carry = @byte_value.odd?
            @byte_value = @byte_value >> 1
            @n, @hc = false, false
        end

        def rotate_left
            self.shift_left
            @byte_value += 1 if @carry
        end

        def rotate_left_trough_carry
            v = @carry
            self.shift_left
            @byte_value += 1 if v
        end

        def rotate_right
            self.shift_right
            self.set_bit(7) if @carry
        end

        def rotate_right_trough_carry
            v = @carry
            self.shift_right
            self.set_bit(7) if @carry
        end

        def exchange reg8
            v = @byte_value
            self.store_byte_value(reg8.byte_value)
            reg8.store_byte_value(v)
        end

        def copy reg8
            self.store_byte_value(reg8.byte_value)
        end

        def store_byte_value bv
            fail if bv < 0 || bv >= MAX8
            @byte_value = bv
        end

        def increase
            reg = Register8.new
            reg.store_byte_value(1)
            self.add(reg)
        end

        def decrease
            reg = Register8.new
            reg.store_byte_value(1)
            self.substract(reg)
        end

        def add(reg8)
            @carry = (self.byte_value + reg8.byte_value >= max_byte_value)
            h1, l1 = self.to_4_bit_pair
            h2, l2 = reg8.to_4_bit_pair
            @hc = (l1 + l2 >= max_byte_value(4))
            @n = false
            self.store(self.two_complement + reg8.two_complement)
        end

        def substract(reg8)
            @carry = (reg8.byte_value > self.byte_value)
            h1, l1 = self.to_4_bit_pair
            h2, l2 = reg8.to_4_bit_pair
            @hc = (l2 > l1)
            @n = true
            self.store(self.two_complement - reg8.two_complement)
        end
    end

    class Flag8 < Register8
        def flag_c
            self.bit?(0)
        end

        def flag_c= value
            self.set_bit(0, value)
        end

        def flag_n
            self.bit?(1)
        end

        def flag_n= value
            self.set_bit(1, value)
        end

        def flag_pv
            self.bit?(2)
        end

        def flag_pv= value
            self.set_bit(2, value)
        end

        def flag_hc
            self.bit?(4)
        end

        def flag_hc= value
            self.set_bit(4, value)
        end

        def flag_z
            self.bit?(6)
        end

        def flag_z= value
            self.set_bit(6, value)
        end

        def flag_s
            self.bit?(7)
        end

        def flag_s= value
            self.set_bit(7, value)
        end

        def s_z reg
            self.flag_s = reg.negative?
            self.flag_z = reg.two_complement.zero?
            self.flags_3_5(reg)
        end

        def s_z_p reg
            self.s_z(reg)
            self.parity(reg)
        end

        def s_z_v_hc_n reg
            self.flag_pv = reg.overflow
            self.flag_hc = reg.hc
            self.flag_n = reg.n
            self.s_z(reg)
        end

        def s_z_v_hc_n_c reg
            self.s_z_v_hc_n(reg)
            self.flag_c = reg.carry
        end

        def s_z_p_hc_n_c reg
            self.s_z_p(reg)
            self.hc_n_c(reg)
        end

        def parity reg
            self.flag_pv = reg.to_s(2).count('1').even?
        end

        def hc_n_c reg
            self.flag_hc, self.flag_c, self.flag_n = reg.hc, reg.carry, reg.n
            self.flags_3_5(reg)
        end

        def flags_3_5 reg
            self.set_bit(3, reg.bit?(3))
            self.set_bit(5, reg.bit?(5))
        end
    end

    class Register16 < GeneralRegister
        attr_reader :high, :low, :overflow, :hc, :carry, :n

        def initialize h = Register8.new, l = Register8.new
            @high, @low = h, l
            @overflow, @hc, @carry, @n = false, false, false, false
            super(16)
        end

        def byte_value
            @high.byte_value * MAX8 + @low.byte_value
        end

        def set_bit(b, value = true)
            fail if b < 0 || b > 15
            if b < 8
                @low.set_bit(b, value)
            else
                @high.set_bit(b - 8, value)
            end
        end

        def copy reg16
            @high.copy(reg16.high)
            @low.copy(reg16.low)
        end

        def exchange reg16
            @low.exchange(reg16.low)
            @high.exchange(reg16.high)
        end

        def negative?
            @high.negative?
        end

        def store_byte_value bv
            h, l = bv.divmod MAX8
            @high.store_byte_value(h)
            @low.store_byte_value(l)
        end

        def increase
            reg = Register16.new
            reg.store_byte_value(1)
            self.add(reg)
        end

        def decrease
            reg = Register16.new
            reg.store_byte_value(1)
            self.substract(reg)
        end

        def add(reg16)
            @carry = (self.byte_value + reg16.byte_value >= MAX16)
            @hc = (@low.byte_value + reg16.low.byte_value >= MAX8)
            @n = false
            self.store(self.two_complement + reg16.two_complement)
        end

        def substract(reg16)
            @carry = (reg16.byte_value > self.byte_value)
            @hc = (reg16.low.byte_value > @low.byte_value)
            @n = true
            self.store(self.two_complement - reg16.two_complement)
        end
    end

    class Memory
        def initialize size
            @memory = Array.new(size) { Register8.new }
        end

        def load data
            fail if data.size && data.size > @memory.size
            data.each_with_index { |v, i| @memory[i].store_byte_value(v) }
        end

        def load_rom filename
            f = File.new(filename)
            fail unless filename.end_with?('.rom')
            fail if f.size > @memory.size
            self.load(f.each_byte)
            f.close
        end

        def read8 reg16
            @memory[reg16.byte_value]
        end

        def read8_indexed reg16, reg8
            reg = Register16.new
            reg.store(reg16.byte_value + reg8.two_complement)
            self.read8(reg)
        end

        def read16 reg16
            h = Register16.new
            h.store(reg16.two_complement + 1)
            Register16.new(self.read8(h), self.read8(reg16))
        end
    end

    class Z80
        attr_reader :memory, :bc, :de, :hl, :af, :pc, :sp, :ix, :iy
        attr_accessor :state_duration

        def initialize
            @a, @b, @c, @d, @e, @h, @l, @i, @r = Array.new(9) { Register8.new }
            @a’, @b’, @c’, @d’, @e’, @h’, @l’ = Array.new(7) { Register8.new }
            @f, @f’ = [Flag8.new, Flag8.new]
            @bc = Register16.new(@b, @c)
            @de = Register16.new(@d, @e)
            @hl = Register16.new(@h, @l)
            @af = Register16.new(@a, @f)
            @pc, @sp, @ix, @iy = Array.new(4) { Register16.new }
            @sp.store_byte_value(0xFFFF)
            @x = @y = 0
            @memory = Memory.new(MAX16)
            @state_duration, @t_states = 0.1, 4
            @iff1, @iff2, @can_execute = false, false, true
            @mode = 0
            @address_bus = Register16.new
            @data_bus = Register8.new
            @nonmaskable_interrupt_flag, @maskable_interrupt_flag = false, false
        end

        def to_s
            "BC #{@bc}, DE #{@de}, HL #{@hl}, AF #{@af}, PC #{@pc}, SP #{@sp}, IX #{@ix}, IY #{@iy}, I #{@i}, R #{@r}, M #{@mode}, IFF1 #{@iff1}"
        end

        def memory_refresh
            @r.increase
            @r.store_byte_value(0) if @r.byte_value >= MAX7
        end

        def fetch_opcode
            memory_refresh
            self.next8
        end

        def next8
            val = @memory.read8(@pc)
            @pc.increase
            val
        end

        def next16
            val = @memory.read16(@pc)
            @pc.increase
            @pc.increase
            val
        end

        def push16
            @sp.decrease
            @sp.decrease
            @memory.read16(@sp)
        end

        def pop16
            val = @memory.read16(@sp)
            @sp.increase
            @sp.increase
            val
        end

        def run
            loop do
                t = Time.now
                if @nonmaskable_interrupt_flag
                    @nonmaskable_interrupt_flag = false
                    nonmaskable_interrupt
                elsif @maskable_interrupt_flag && @iff1
                    @maskable_interrupt_flag = false
                    maskable_interrupt
                elsif @can_execute
                    execute self.fetch_opcode
                end
                sleep([t + @t_states * @state_duration - Time.now, 0].max) / 1000.0
            end
        end

        def nonmaskable_interrupt
            @t_states = 11
            @iff1, @iff2 = false, @iff1
            self.push16.copy(@pc)
            @pc.copy(0x66)
        end

        def maskable_interrupt
            case @mode
            when 0
                #TODO: wait 2 cycles for interrupting device to write to data_bus
                memory_refresh
                execute @data_bus
                @t_states += 2
            when 1
                @t_states = 13
                self.push16.copy(@pc)
                @pc.store_byte_value(0x38)
            when 2
                @t_states = 19
                self.push16.copy(@pc)
                @pc.copy(@memory.read16(Register16.new(@i, @data_bus)))
            else
                fail
            end
        end

        def decode_register8 reg8, pos = 3, t = 3
            v = reg8.byte_value >> pos & 0x07
            @t_states += t if v == 0x06
            [@b, @c, @d, @e, @h, @l, @memory.read8(@hl), @a][v]
        end

        def decode_register16 reg16, pos = 4
            [@bc, @de, @hl, @sp][reg16.byte_value >> pos & 0x03]
        end

        def execute opcode
            @t_states = 4
            case opcode.byte_value
            when 0x00 #NOP
            when 0x01, 0x11, 0x21, 0x31 #LD dd,nn
                @t_states = 10
                self.decode_register16(opcode).copy(self.next16)
            when 0x02 #LD (BC),A
                @t_states = 7
                @memory[@bc.two_complement].copy(@a)
            when 0x03, 0x13, 0x23, 0x33 #INC ss
                @t_states = 6
                reg = self.decode_register16(opcode)
                reg.increase
            when 0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C #INC r
                reg = self.decode_register8(opcode, 3, 7)
                reg.increase
                @f.s_z_v_hc_n(reg)
            when 0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D #DEC r
                reg = self.decode_register8(opcode, 3, 7)
                reg.decrease
                @f.s_z_v_hc_n(reg)
            when 0x06 #LD B,NN
                @t_states = 7
                @b.copy(self.next8)
                op_size = 2
            when 0x07 #RLCA
                @a.rotate_left
                @f.hc_n_c(@a)
            when 0x08 #EX AF,AF’
                @a.exchange(@a’)
                @f.exchange(@f’)
            when 0x09 #ADD HL,BC
                @t_states = 11
                @hl.add(@bc)
                @f.hc_n_c(@hl)
            when 0x0A #LD A,(BC)
                @t_states = 7
                @a.copy(@memory.read8(@bc))
            when 0x0B, 0x1B, 0x2B, 0x3B #DEC ss
                @t_states = 6
                reg = self.decode_register16(opcode)
                reg.decrease
            when 0x0E #LD C,NN
                @t_states = 7
                @c.copy(self.next8)
            when 0x0F #RRCA
                @a.rotate_right
                @f.hc_n_c(@a)
            when 0x10 #DJNZ NN
                reg = self.next8
                @b.decrease
                if @b.byte_value.nonzero?
                    @pc.store(@pc.byte_value + reg.two_complement)
                    @t_states = 13
                else
                    @t_states = 8
                end
            when 0x12 #LD (DE),A
                @t_states = 7
                @memory.read8(@de).copy(@a)
            when 0x16 #LD D,NN
                @t_states = 7
                @d.store(self.next8)
            when 0x17 #RLA
                @a.carry = @f.flag_c
                @a.rotate_left_trough_carry
                @f.hc_n_c(@a)
            when 0x18 #JR NN
                @t_states = 12
                @pc.store(@pc.byte_value + self.next8.two_complement)
            when 0x19 #ADD HL,DE
                @t_states = 11
                @hl.add(@de)
                @f.hc_n_c(@hl)
            when 0x1A #LD A,(DE)
                @t_states = 7
                @a.copy(@memory.read8(@de))
            when 0x1E #LD E,NN
                @t_states = 7
                @e.copy(self.next8)
            when 0x1F #RRA
                @a.carry = @f.flag_c
                @a.rotate_right_trough_carry
                @f.hc_n_c(@a)
            when 0x20 #JR NZ,NN
                reg = self.next8
                if @f.flag_z
                    @t_states = 7
                else
                    @pc.store(@pc.byte_value + reg.two_complement)
                    @t_states = 12
                end
            when 0x22 #LD (HHLL),HL
                @t_states = 16
                @memory.read16(self.next16).copy(@hl)
            when 0x26 #LD H,NN
                @t_states = 7
                @h.copy(self.next8)
            when 0x27 #DAA
                q, r = @a.to_4_bit_pair
                c = @f.flag_c
                h = @f.flag_hc
                if c == false && h == false && q == 0x90 && r == 0x09
                    v = 0x00
                    @f.flag_c = false
                elsif c == false && h == false && q ==0x08 && r == 0xAF
                    v = 0x06
                    @f.flag_c = false
                elsif c == false && h == true && q == 0x09 && r == 0x03
                    v = 0x06
                    @f.flag_c = false
                elsif c == false && h == false && q == 0xAF && r == 0x09
                    v = 0x60
                    @f.flag_c = true
                elsif c == false && h == false && q == 0x9F && r == 0xAF
                    v = 0x66
                    @f.flag_c = true
                elsif c == false && h == true && q == 0xAF && r == 0x03
                    v = 0x66
                    @f.flag_c = true
                elsif c == true && h == false && q == 0x02 && r == 0x09
                    v = 0x60
                    @f.flag_c = true
                elsif c == true && h == false && q == 0x02 && r == 0xAF
                    v = 0x66
                    @f.flag_c = true
                elsif c == false && h == true && q == 0x03 && r == 0x03
                    v = 0x66
                    @f.flag_c = true
                elsif c == false && h == false && q == 0x09 && r == 0x09
                    v = 0x00
                    @f.flag_c = false
                elsif c == false && h == true && q == 0x08 && r == 0x6F
                    v = 0xFA
                    @f.flag_c = false
                elsif c == true && h == false && q == 0x7F && r == 0x09
                    v = 0xA0
                    @f.flag_c = true
                elsif c == true && h == true && q == 0x67 && r == 0x6F
                    v = 0x9A
                    @f.flag_c = true
                end
                @f.s_z_p(@a)
            when 0x28 #JR Z,NN
                reg = self.next8
                if @f.flag_z
                    @pc.store(@pc.byte_value + reg.two_complement)
                    @t_states = 12
                else
                    @t_states = 7
                end
            when 0x29 #ADD HL,HL
                @t_states = 11
                @hl.add(@hl)
                @f.hc_n_c(@hl)
            when 0x2A #LD HL,(HHLL)
                @t_states = 16
                @hl.copy(@memory.read16(self.next16))
            when 0x2E #LD L,NN
                @t_states = 7
                @l.copy(self.next8)
            when 0x2F #CPL
                @a.negate
                @f.flag_n, @f.flag_hc = true
                @f.flags_3_5(@a)
            when 0x30 #JR NC,NN
                reg = self.next8
                if @f.flag_c
                    @t_states = 7
                else
                    @pc.store(@pc.byte_value + reg.two_complement)
                    @t_states = 12
                end
            when 0x32 #LD (HHLL),A
                @t_states = 16
                @memory.read8(self.next16).copy(@a)
            when 0x36 #LD (HL),NN
                @t_states = 10
                @memory.read8(@hl).copy(self.next8)
            when 0x37 #SCF
                @f.flag_c, @f.flag_n, @f.flag_hc = true, false, false
            when 0x38 #JR C,NN
                reg = self.next8
                if @f.flag_c
                    @pc.store(@pc.byte_value + reg.two_complement)
                    @t_states = 12
                else
                    @t_states = 7
                end
            when 0x39 #ADD HL,SP
                @t_states = 11
                @hl.add(@sp)
                @f.hc_n_c(@hl)
            when 0x3A #LD A,(HHLL)
                @t_states = 13
                @a.copy(@memory.read8(self.next16))
            when 0x3E #LD A,NN
                @t_states = 7
                @a.copy(self.next8)
            when 0x3F #CCF
                @f.flag_hc = @f.flag_c
                @f.flag_c = !@f.flag_c
                @f.flag_n = false
            when 0x40..0x49, 0x4A..0x4F, 0x50..0x59, 0x5A..0x5F, 0x60..0x69, 0x6A..0x6F, 0x70..0x75, 0x77..0x79, 0x7A..0x7F #LD r,r
                self.decode_register8(opcode).copy(self.decode_register8(opcode, 0))
            when 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87 #ADD A,r
                @a.add(self.decode_register8(opcode, 0))
                @f.flag_c = @a.carry
                @f.s_z_v_hc_n(@a)
            when 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F #ADC A,r
                @a.add(self.decode_register8(opcode, 0))
                @a.increase if @f.flag_c
                @f.flag_c = @a.carry
                @f.s_z_v_hc_n(@a)
            when 0x90, 0x91, 0x92, 0x93, 0x94, 0x94, 0x96, 0x97 #SUB A,r
                @a.substract(self.decode_register8(opcode, 0))
                @f.flag_c = @a.carry
                @f.s_z_v_hc_n(@a)
            when 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F #SBC A,r
                @a.substract(self.decode_register8(opcode, 0))
                @a.decrease if @f.flag_c
                @f.flag_c = @a.carry
                @f.s_z_v_hc_n(@a)
            when 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7 #AND A,r
                @a.store(@a.byte_value & self.decode_register8(opcode, 0).byte_value)
                @f.s_z_p(@a)
                @f.flag_n, @f.flag_c, @f.flag_hc = false, false, true
            when 0xA8, 0xA9,0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF #XOR A,r
                @a.store(@a.byte_value ^ self.decode_register8(opcode, 0).byte_value)
                @f.s_z_p(@a)
                @f.flag_hc, @f.flag_n, @f.flag_c = false, false, false
            when 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7 #OR A,r
                @a.store(@a.byte_value | self.decode_register8(opcode, 0).byte_value)
                @f.s_z_p(@a)
                @f.flag_hc, @f.flag_n, @f.flag_c = false, false, false
            when 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF #CP A,r
                reg = Register8.new
                reg.copy(@a)
                reg.substract(self.decode_register8(opcode, 0))
                @f.s_z_v_hc_n_c(reg)
                @f.flags_3_5(@a)
            when 0xC0 #RET NZ
                if @f.flag_z
                    @t_states = 11
                else
                    @pc.copy(self.pop16)
                    @t_states = 15
                end
            when 0xC1, 0xD1, 0xE1, 0xF1 #POP qq
                @t_states = 10
                [@bc, @de, @hl, @af][opcode & 0x30].copy(self.pop16)
            when 0xC2 #JP NZ,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if !@f.flag_z
            when 0xC3 #JP HHLL
                @t_states = 10
                @pc.copy(self.next16)
            when 0xC4 #CALL NZ,HHLL
                reg = self.next16
                if @f.flag_z
                    @t_states = 10
                else
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                end
            when 0xC5, 0xD5, 0xE5, 0xF5 #PUSH qq
                @t_states = 10
                self.push16.copy([@bc, @de, @hl, @af][opcode & 0x30])
            when 0xC6 #ADD A,NN
                @t_states = 7
                @a.add(self.next8)
                @f.flag_c = @a.carry
                @f.s_z_v_hc_n(@a)
            when 0xC7 #RST 00
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x00)
            when 0xC8 #RET Z
                if @f.flag_z
                    @t_states = 15
                    @pc.copy(self.pop16)
                else
                    @t_states = 11
                end
            when 0xC9 #RET
                @t_states = 10
                @pc.copy(self.pop16)
            when 0xCA #JP Z,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if @f.flag_z
            when 0xCB #CB
                opcode = self.fetch_opcode
                case opcode.byte_value
                when 0x00..0x3F
                    reg = self.decode_register8(opcode, 3, 7)
                    case opcode.byte_value
                    when 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,0x06, 0x07 #RLC r
                        reg.rotate_left
                    when 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F #RRC r
                        reg.rotate_right
                    when 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 #RL r
                        reg.carry = @f.flag_c
                        reg.rotate_left_trough_carry
                    when 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F #RR r
                        reg.carry = @f.flag_c
                        reg.rotate_right_trough_carry
                    when 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27 #SLA r
                        reg.shift_left
                    when 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F #SRA r
                        reg.carry = reg.negative?
                        reg.rotate_right_trough_carry
                    when 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 #SLL r
                        reg.carry = true
                        reg.rotate_left_trough_carry
                    when 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F #SRL r
                        reg.carry = false
                        reg.rotate_right_trough_carry
                    end
                    @f.s_z_p_hc_n_c(reg)
                when 0x40..0x7F #BIT b,r
                    @f.flag_z = !(self.decode_register8(opcode, 0).bit?(opcode.byte_value >> 3 & 0x07))
                    @f.flag_hc, @f.flag_n = true, false
                when 0x80..0xBF #RES b,r
                    self.decode_register8(opcode, 0, 7).reset_bit(opcode.byte_value >> 3 & 0x07)
                when 0xC0..0xFF #SET b,r
                    self.decode_register8(opcode, 0, 7).set_bit(opcode.byte_value >> 3 & 0x07)
                end
                @t_states += 4
            when 0xCC #CALL Z,HHLL
                reg = self.next16
                if @f.flag_z
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                else
                    @t_states = 10
                end
            when 0xCD #CALL HHLL
                @t_states = 17
                reg = self.next16
                self.push16.copy(@pc)
                @pc.copy(reg)
            when 0xCE #ADC A,NN
                @t_states = 7
                @a.add(self.next8)
                @a.increase if @f.flag_c
            when 0xCF #RST 08
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x08)
            when 0xD0 #RET NC
                if @f.flag_c
                    @t_states = 11
                else
                    @t_states = 15
                    @pc.copy(self.pop16)
                end
            when 0xD2 #JP NC,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if !@f.flag_c
            when 0xD3 #OUT (NN),A
                @t_states = 11
                @address_bus.copy(Register16.new(@a, self.next8))
                @data_bus.copy(@a)
                #TODO: write one byte from data_bus to device in address_bus
            when 0xD4 #CALL NC,HHLL
                reg = self.next16
                if @f.flag_c
                    @t_states = 10
                else
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                end
            when 0xD6 #SUB A,NN
                @t_states = 7
                @a.substract(self.next8)
                @f.flag_c = @a.carry
                @f.s_z_v_hc_n(@a)
            when 0xD7 #RST 10
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x10)
            when 0xD8 #RET C
                if @f.flag_c
                    @t_states = 15
                    @pc.copy(self.pop16)                    
                else
                    @t_states = 11
                end
            when 0xD9 #EXX
                @b.exchange(@b’)
                @c.exchange(@c’)
                @d.exchange(@d’)
                @e.exchange(@e’)
                @h.exchange(@h’)
                @l.exchange(@l’)
            when 0xDA #JP C,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if @f.flag_c
            when 0xDB #IN A,(NN)
                @t_states = 11
                @address_bus.copy(Register16.new(@a, self.next8))
                #TODO: read one byte from device in address_bus to data_bus
                @a.copy(@data_bus)
            when 0xDC #CALL C,HHLL
                reg = self.next16
                if @f.flag_c
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                else
                    @t_states = 10
                end
            when 0xDD #DD
                opcode = self.fetch_opcode
                case opcode.byte_value
                when 0x09, 0x19, 0x29, 0x39 #ADD IX,pp
                    @t_states = 15
                    @ix.add([@bc, @de, @ix, @sp][opcode.byte_value & 0x30])
                    @f.hc_n_c(@ix)
                when 0x21 #LD IX,nn
                    @t_states = 14
                    @ix.copy(self.next16)
                when 0x22 #LD (nn),IX
                    @t_states = 20
                    @memory.read16(self.next16).copy(@ix)
                when 0x23 #INC IX
                    @t_states = 10
                    @ix.increase
                when 0x2A #LD IX,(nn)
                    @t_states = 20
                    @ix.copy(@memory.read16(self.next16))
                when 0x2B #DEC IX 
                    @t_states = 10
                    @ix.decrease
                when 0x34 #INC (IX+d)
                    @t_states = 23
                    reg = @memory.read8_indexed(@ix, self.next8)
                    reg.increase
                    @f.s_z_v_hc_n(reg)
                when 0x35 #DEC (IX+d)
                    @t_states = 23
                    reg = @memory.read8_indexed(@ix, self.next8)
                    reg.decrease
                    @f.s_z_v_hc_n(reg)
                when 0x36 #LD (IX+d),n
                    @t_states = 19
                    @memory.read8_indexed(@ix, self.next8).store(self.next8)
                when 0x46, 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x7E #LD r,(IX+d)
                    @t_states = 19
                    self.decode_register8(opcode).copy(@memory.read8_indexed(@ix, self.next8))
                when 0x86, 0x8E #ADD/ADC A,(IX+d)
                    @t_states = 19
                    @a.add(@memory.read8_indexed(@ix, self.next8))
                    @a.increase if opcode.byte_value == 0x8E && @f.flag_c
                    @f.s_z_v_hc_n_c(@a)
                when 0x96, 0x9E #SUB/SBC A,(IX+d)
                    @t_states = 19
                    @a.substract(@memory.read8_indexed(@ix, self.next8))
                    @a.decrease if opcode.byte_value == 0x9E && @f.flag_c
                    @f.s_z_v_hc_n_c(@a)
                when 0xA6 #AND A,(IX+d)
                    @t_states = 19
                    @a.store(@a.two_complement & @memory.read8_indexed(@ix, self.next8))
                    @f.s_z_p(@a)
                    @f.flag_n, @f.flag_c, @f.flag_hc = false, false, true
                when 0xAE #XOR A,(IX+d)
                    @t_states = 19
                    @a.store(@a.two_complement ^ @memory.read8_indexed(@ix, self.next8))
                    @f.s_z_p(@a)
                    @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false
                when 0xB6 #OR A,(IX+d)
                    @t_states = 19
                    @a.store(@a.two_complement | @memory.read8_indexed(@ix, self.next8))
                    @f.s_z_p(@a)
                    @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false
                when 0xBE #CP A,(IX+d)
                    @t_states = 19
                    reg = Register8.new
                    reg.copy(@a)
                    reg.substract(@memory.read8_indexed(@ix, self.next8))
                    @f.s_z_v_hc_n_c(reg)
                    @f.flags_3_5(@a)
                when 0xCB #DDCB
                    opcode = self.fetch_opcode
                    reg = @memory.read8_indexed(@ix, self.next8)
                    case opcode.byte_value
                    when 0x06 #RLC (IX+d)	
                        @t_states = 23
                        reg.rotate_left
                        @f.s_z_p_hc_n_c(reg)
                    when 0x0E #RRC (IX+d)
                        @t_states = 23
                        reg.rotate_right
                        @f.s_z_p_hc_n_c(reg)
                    when 0x16 #RL (IX+d)
                        @t_states = 23
                        reg.rotate_left_trough_carry
                        @f.s_z_p_hc_n_c(reg)
                    when 0x1E #RR (IX+d)
                        @t_states = 23
                        reg.rotate_right_trough_carry
                        @f.s_z_p_hc_n_c(reg)
                    when 0x26 #SLA (IX+d)
                        @t_states = 23
                        reg.shift_left
                        @f.s_z_p_hc_n_c(reg)
                    when 0x2E #SRA (IX+d)
                        @t_states = 23
                        reg.carry = reg.negative?
                        reg.rotate_right_trough_carry                    
                        @f.s_z_p_hc_n_c(reg)
                    when 0x36 #SLL (IX+d)
                        @t_states = 23
                        reg.carry = true
                        reg.rotate_left_trough_carry                    
                        @f.s_z_p_hc_n_c(reg)
                    when 0x3E #SRL (IX+d)
                        @t_states = 23
                        reg.carry = false
                        reg.rotate_right_trough_carry                    
                        @f.s_z_p_hc_n_c(reg)
                    when 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x76, 0x7E #BIT b,(IX+d)
                        @t_states = 20
                        @f.flag_z = !(reg.bit?(opcode.byte_value >> 3 & 0x07))
                        @f.flag_hc, @f.flag_n = true, false
                    when 0x86, 0x8E, 0x96, 0x9E, 0xA6, 0xAE, 0xB6, 0xBE #RES b,(IX+d)
                        @t_states = 23
                        reg.reset_bit(opcode.byte_value >> 3 & 0x07)
                    when 0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE #SET b,(IX+d)
                        @t_states = 20
                        reg.set_bit(opcode.byte_value >> 3 & 0x07)
                    end
                when 0xE1 #POP IX
                    @t_states = 19
                    @ix.copy(self.pop16)
                when 0xE3 #EX (SP),IX
                    @t_states = 23
                    @ix.exchange(@memory.read16(@sp))
                when 0xE5 #PUSH IX
                    @t_states = 15
                    self.push16.copy(@ix)
                when 0xE9 #JP (IX)
                    @t_states = 8
                    @pc.copy(@ix)
                when 0xF9 #LD SP,IX
                    @t_states = 10
                    @sp.copy(@ix)
                end
            when 0xDE #SBC A,NN
                @t_states = 7
                @a.substract(self.next8)
                @a.decrease if @f.carry
                @f.s_z_v_hc_n_c(@a)
            when 0xDF #RST 18
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x18)
            when 0xE0 #RET PO
                if @f.flag_pv
                    @t_states = 11
                else
                    @t_states = 15
                    @pc.copy(self.pop16)
                end
            when 0xE2 #JP PO,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if !@f.flag_pv
            when 0xE3 #EX (SP),HL
                @t_states = 19
                @hl.exchange(@memory.read16(@sp))
            when 0xE4 #CALL PO,HHLL
                reg = self.next16
                if @f.flag_pv
                    @t_states = 10
                else
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                end
            when 0xE6 #AND A,NN
                @t_states = 7
                @a.store(@a.two_complement & self.next8.two_complement)
                @f.s_z(@a)
                @f.flag_pv, @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false, true
            when 0xE7 #RST 20
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x20)
            when 0xE8 #RET PE
                if @f.flag_pv
                    @t_states = 15
                    @pc.copy(self.pop16)
                else
                    @t_states = 11
                end
            when 0xE9 #JP (HL)
                @pc.copy(@memory.read16(@hl))
            when 0xEA #JP PE,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if @f.flag_pv
            when 0xEB #EX DE,HL
                @de.exchange(@hl)
            when 0xEC #CALL PE,HHLL
                reg = self.next16
                if @f.flag_pv
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                else
                    @t_states = 10
                end
            when 0xED #ED
                opcode = self.fetch_opcode
                case opcode.byte_value
                when 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x78 #IN r,(C)
                    @t_states = 12
                    reg = self.decode_register8(opcode)
                    @address_bus.copy(@bc)
                    #TODO: read one byte from device in address_bus to data_bus
                    reg.copy(@data_bus)
                    @f.s_z_p(reg)
                    @f.flag_hc, @f.flag_n = false, false
                when 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x79 #OUT (C),r
                    @t_states = 12
                    reg = self.decode_register8(opcode)
                    @address_bus.copy(@bc)
                    @data_bus.copy(reg)
                    #TODO: write one byte from data_bus to device in address_bus
                when 0x42, 0x52, 0x62, 0x72 #SBC HL,ss
                    @t_states = 15
                    @hl.substract(self.decode_register16(opcode))
                    @hl.decrease if @f.flag_c
                    @f.s_z_v_hc_n_c(@hl)
                when 0x43, 0x53, 0x63, 0x73 #LD (nn),dd
                    @t_states = 20
                    @memory.read16(self.next16).copy(self.decode_register16(opcode))
                when 0x44 #NEG
                    @t_states = 8
                    @a.negate
                    @f.s_z_v_hc_n(@a)
                    @f.flag_c = @a.byte_value.nonzero?
                when 0x45 #RETN
                    @t_states = 14
                    @pc.copy(self.pop16)
                    @iff1 = @iff2
                when 0x46 #IM 0
                    @t_states = 8
                    @mode = 0
                when 0x47 #LD I,A
                    @t_states = 9
                    @i.copy(@a)
                when 0x4A, 0x5A, 0x6A, 0x7A #ADC HL,ss
                    @t_states = 15
                    @hl.add(self.decode_register16(opcode))
                    @hl.increase if @f.flag_c
                    @f.s_z_v_hc_n_c(@hl)
                when 0x4B, 0x5B, 0x6B, 0x7B #LD dd,(nn)
                    @t_states = 20
                    @memory.read16(self.decode_register16(opcode)).store(self.next16)
                when 0x4D #RETI
                    @t_states = 14
                    @pc.copy(self.pop16)
                    #TODO: signal devices that interrupt routine is completed
                when 0x4F #LD R,A
                    @r.copy(@a)
                when 0x56 #IM 1
                    @t_states = 8
                    @mode = 1
                when 0x57 #LD A,I
                    @t_states = 9
                    @a.copy(@i)
                    @f.s_z(@a)
                    @f.flag_pv, @f.flag_n = @iff2, false
                when 0x5E #IM 2
                    @t_states = 8
                    @mode = 2
                when 0x5F #LD A,R
                    @t_states = 9
                    @a.copy(@r)
                    @f.s_z(@a)
                    @f.flag_hc, @f.flag_n, @f.flag_pv = false, false, @iff2
                when 0x67 #RRD
                    @t_states = 18
                    reg = @memory.read8(@hl)
                    reg_high4, reg_low4 = reg.to_4_bit_pair
                    a_high4, a_low4 = @a.to_4_bit_pair
                    temp4 = reg_low4
                    @a.store_4_bit_pair(a_high4, reg_low4)
                    reg.store_4_bit_pair(temp, reg_high4)
                    @f.s_z_p(@a)
                    @f.flag_hc, @f.flag_n = false, false
                when 0x6F #RLD
                    @t_states = 18
                    reg = @memory.read8(@hl)
                    reg_high4, reg_low4 = reg.to_4_bit_pair
                    a_high4, a_low4 = @a.to_4_bit_pair
                    temp4 = a_low4
                    @a.store_4_bit_pair(a_high4, reg_high4)
                    reg.store_4_bit_pair(reg_low4, temp4)
                    @f.s_z_p(@a)
                    @f.flag_hc, @f.flag_n = false, false
                when 0xA0, 0xB0 #LDI & LDIR
                    @t_states = 16
                    @memory.read8(@de).copy(@memory.read8(@hl))
                    @de.increase
                    @hl.increase
                    @bc.decrease
                    @f.flag_pv = @bc.byte_value.nonzero?
                    @f.flag_hc, @f.flag_n = false, false
                    if opcode.byte_value == 0xB0 && @f.flag_pv
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                when 0xA1, 0xB1 #CPI & CPIR
                    @t_states = 16
                    reg = Register8.new
                    reg.copy(@a)
                    reg.substract(@memory.read8(@hl))
                    @hl.increase
                    @bc.decrease
                    @f.s_z_v_hc_n(reg)
                    @f.flag_pv = @bc.byte_value.nonzero?
                    @f.flag_n = true
                    if opcode.byte_value == 0xB1 && @f.flag_pv
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                when 0xA2, 0xB2 #INI & INIR
                    @t_states = 16
                    @address_bus.copy(@bc)
                    #TODO: read one byte from device in address_bus to data_bus
                    @address_bus.copy(@hl)
                    @memory.read8(@hl).copy(@data_bus)
                    @b.decrease
                    @hl.increase
                    @f.flag_z(@b)
                    @f.flag_n = true
                    if opcode.byte_value == 0xB2 && @f.flag_z
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                when 0xA3, 0xB3 #OUTI & OTIR
                    @t_states = 16
                    @b.decrease
                    @address_bus.copy(@bc)
                    @data_bus.copy(@memory.read8(@hl))
                    #TODO: write one byte from address_bus to device
                    @hl.increase
                    @f.flag_z(@b)
                    @f.flag_n = true
                    if opcode.byte_value == 0xB3 && @f.flag_z
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                when 0xA8, 0xB8 #LDD & LDDR
                    @t_states = 16
                    @memory.read8(@de).copy(@memory.read8(@hl))
                    @de.decrease
                    @hl.decrease
                    @bc.decrease
                    @f.flag_pv = @bc.byte_value.nonzero?
                    @f.flag_hc, @f.flag_n = false, false
                    if opcode.byte_value == 0xB8 && @f.flag_pv
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                when 0xA9, 0xB9 #CPD & CPDR
                    @t_states = 16
                    reg = Register8.new
                    reg.copy(@a)
                    reg.substract(@memory.read8(@hl))
                    @hl.decrease
                    @bc.decrease
                    @f.s_z_v_hc_n(reg)
                    @f.flag_pv = @bc.byte_value.nonzero?
                    if opcode.byte_value == 0xB9 && @f.flag_pv
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                when 0xAA, 0xBA #IND & INDR
                    @t_states = 16
                    @address_bus.copy(@bc)
                    #TODO: read one byte from device in address_bus to data_bus
                    @address_bus.copy(@hl)
                    @memory.read8(@hl).copy(@data_bus)
                    @b.decrease
                    @hl.decrease
                    @f.flag_z(@b)
                    @f.flag_n = true
                    if opcode.byte_value == 0xBA && @f.flag_z
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                when 0xAB, 0xBB #OUTD & OTDR
                    @t_states = 16
                    @b.decrease
                    @address_bus.copy(@bc)
                    @data_bus.copy(@memory.read8(@hl))
                    #TODO: write one byte from address_bus to device
                    @hl.decrease
                    @f.flag_z(@b)
                    @f.flag_n = true
                    if opcode.byte_value == 0xB3 && @f.flag_z
                        @t_states = 21
                        @pc.decrease
                        @pc.decrease
                    end
                end
            when 0xEE #XOR A,NN
                @t_states = 7
                @a.store(@a.two_complement ^ self.next8.two_complement)
                @f.s_z_p(@a)
                @f.flag_hc, @f.flag_n, @f.flag_c = false, false, false
            when 0xEF #RST 28
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x28)
            when 0xF0 #RET P
                if @f.flag_s
                    @t_states = 11
                else
                    @t_states = 15
                    @pc.copy(self.pop16)
                end
            when 0xF2 #JP P,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if !@f.flag_s
            when 0xF3 #DI
                @iff1, @iff2 = false, false
            when 0xF4 #CALL P,HHLL
                reg = self.next16
                if @f.flag_s
                    @t_states = 10
                else
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                end
            when 0xF6 #OR A,NN
                @t_states = 7
                @a.store(@a.two_complement | self.next8.two_complement)
                @f.s_z(@a)
                @f.flag_pv, @f.flag_hc, @f.flag_n, @f.flag_c = false, false, false, false
            when 0xF7 #RST 30
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x30)
            when 0xF8 #RET M
                if @f.flag_s
                    @t_states = 15
                    @pc.copy(self.pop16)
                else
                    @t_states = 11
                end
            when 0xF9 #LD SP,HL
                @sp.copy(@hl)
            when 0xFA #JP M,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if @f.flag_s
            when 0xFB #EI
                @iff1, @iff2 = true
            when 0xFC #CALL M,HHLL
                reg = self.next16
                if @f.flag_s
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                else
                    @t_states = 10
                end
            when 0xFD #FD
                opcode = self.fetch_opcode
                case opcode.byte_value
                when 0x09, 0x19, 0x29, 0x39 #ADD IY,rr
                    @t_states = 15
                    @iy.add([@bc, @de, @iy, @sp][opcode.byte_value & 0x30])
                    @f.hc_n_c(@iy)
                when 0x21 #LD IY,nn
                    @t_states = 14
                    @iy.copy(self.next16)
                when 0x22 #LD (nn),IY
                    @t_states = 20
                    @memory.read16(self.next16).copy(@iy)
                when 0x23 #INC IY
                    @t_states = 10
                    @iy.increase
                when 0x2A #LD IY,(nn)
                    @t_states = 20
                    @iy.copy(@memory.read16(self.next16))
                when 0x2B #DEC IY
                    @t_states = 10
                    @iy.decrease
                when 0x34 #INC (IY+d)
                    @t_states = 23
                    reg = @memory.read8_indexed(@iy, self.next8)
                    reg.increase
                    @f.s_z_v_hc_n(reg)
                when 0x35 #DEC (IY+d)
                    @t_states = 23
                    reg = @memory.read8_indexed(@iy, self.next8)
                    reg.decrease
                    @f.s_z_v_hc_n(reg)
                when 0x36 #LD (IY+d),n
                    @t_states = 19
                    @memory.read8_indexed(@iy, self.next8).store(self.next8)
                when 0x46, 0x56, 0x66, 0x4E, 0x5E, 0x6E, 0x7E #LD r,(IY+d)
                    @t_states = 19
                    reg1 = self.decode_register8(opcode)
                    reg1.copy(@memory.read8_indexed(@iy, self.next8))
                when 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77 #LD (IY+d),r
                    @t_states = 19
                    reg1 = self.decode_register8(opcode, 0)
                    @memory.read8_indexed(@iy, self.next8).copy(reg1)
                when 0x86, 0x8E #ADD A,(IY+d) & ADC A,(IY+d)
                    @t_states = 19
                    @a.add(@memory.read8_indexed(@iy, self.next8))
                    @a.increase if opcode.byte_value == 0x8E && @f.flag_c
                    @f.flag_c = @a.carry
                    @f.s_z_v_hc_n(@a)
                when 0x96, 0x9E #SUB A,(IY+d) & SBC A,(IY+d)
                    @t_states = 19
                    @a.substract(@memory.read8_indexed(@iy, self.next8))
                    @a.decrease if opcode.byte_value == 0x9E && @f.flag_c
                    @f.flag_c = @a.carry
                    @f.s_z_v_hc_n(@a)
                when 0xA6 #AND A,(IY+d)
                    @t_states = 19
                    @a.store(@a.two_complement & @memory.read8_indexed(@iy, self.next8).two_complement)
                    @f.s_z_p(@a)
                    @f.flag_n, @f.flag_c, @f.flag_hc = false, false, true
                when 0xAE #XOR A,(IY+d)
                    @t_states = 19
                    @a.store(@a.two_complement ^ @memory.read8_indexed(@iy, self.next8).two_complement)
                    @f.s_z_p(@a)
                    @f.flag_hc, @f.flag_n, @f.flag_c = false, false, false
                when 0xB6 #OR A,(IY+d)
                    @t_states = 19
                    @a.store(@a.two_complement | @memory.read8_indexed(@iy, self.next8).two_complement)
                    @f.s_z_p(@a)
                    @f.flag_hc, @f.flag_n, @f.flag_c = false, false, false
                when 0xCB #FDCB
                    reg = @memory.read8_indexed(@iy, self.next8)
                    opcode = self.fetch_opcode
                    case opcode.byte_value
                    when 0x06 #RLC (IY+d)	
                        @t_states = 23
                        reg.rotate_left
                        @f.s_z_p_hc_n_c(reg)
                    when 0x0E #RRC (IY+d)
                        @t_states = 23
                        reg.rotate_right
                        @f.s_z_p_hc_n_c(reg)
                    when 0x16 #RL (IY+d)
                        @t_states = 23
                        reg.rotate_left_trough_carry
                        @f.s_z_p_hc_n_c(reg)
                    when 0x1E #RR (IY+d)
                        @t_states = 23
                        reg.rotate_right_trough_carry
                        @f.s_z_p_hc_n_c(reg)
                    when 0x26 #SLA (IY+d)
                        @t_states = 23
                        reg.shift_left
                        @f.s_z_p_hc_n_c(reg)
                    when 0x2E #SRA (IY+d)
                        @t_states = 23
                        reg.carry = reg.negative?
                        reg.rotate_right_trough_carry                    
                        @f.s_z_p_hc_n_c(reg)
                    when 0x36 #SLL (IY+d)
                        @t_states = 23
                        reg.carry = true
                        reg.rotate_left_trough_carry                    
                        @f.s_z_p_hc_n_c(reg)
                    when 0x3E #SRL (IY+d)
                        @t_states = 23
                        reg.carry = false
                        reg.rotate_right_trough_carry                    
                        @f.s_z_p_hc_n_c(reg)
                    when 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x76, 0x7E #BIT b,(IY+d)
                        @t_states = 20
                        @f.flag_z = !(reg.bit?(opcode.byte_value >> 3 & 0x07))
                        @f.flag_hc, @f.flag_n = true, false
                    when 0x86, 0x8E, 0x96, 0x9E, 0xA6, 0xAE, 0xB6, 0xBE #RES b,(IY+d)
                        @t_states = 23
                        reg.reset_bit(opcode.byte_value >> 3 & 0x07)
                    when 0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE #SET b,(IY+d)
                        @t_states = 20
                        reg.set_bit(opcode.byte_value >> 3 & 0x07)
                    end
                when 0xE1 #POP IY
                    @t_states = 14
                    @iy.copy(self.pop16)
                when 0xE3 #EX (SP),IY
                    @t_states = 23
                    @iy.exchange(@memory.read16(@sp))
                when 0xE5 #PUSH IY
                    @t_states = 15
                    self.push16.copy(@iy)
                when 0xE9 #JP (IY)
                    @t_states = 8
                    @pc.copy(@iy)
                when 0xF9 #LD SP,IY
                    @t_states = 10
                    @sp.copy(@iy)
                end
            when 0xFE #CP A,NN
                reg = Register8.new
                reg.copy(@a)
                reg.substract(self.next8)
                @f.s_z_v_hc_n_c(reg)
                @f.flags_3_5(@a)
            when 0xFF #RST 38
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x38)
            end
        end
    end

    class Hardware
        def boot
            root = TkRoot.new { title 'Cristian Mocanu Z80' }
            @canvas = TkCanvas.new(root) do
                place('height' => 256, 'width' => 256, 'x' => 0, 'y' => 0)
            end
            @canvas.pack
            @z80 = Z80.new
            @z80.memory.load_rom('./roms/hc90.rom')
            Thread.new { @z80.run }
            TkAfter.new(5000, -1, proc { draw_screen }).start
            Tk.mainloop
        end

        def point(x, y)
            TkcLine.new(@canvas, x, y, x + 1, y, 'width' => '1')
        end

        def draw_screen
            @z80.maskable_interrupt_flag = true
            reg_bitmap_addr, reg_attrib_addr, reg_y = Register16.new, Register16.new, Register8.new
            reg_address.store_byte_value(0x4000)
            192.times do
                x = 0
                32.times do
                    reg_bitmap = @z80.memory.read8(reg_bitmap_addr)
                    reg_attrib_address.store_byte_value(0x5800 + 32 * reg_y.byte_value / 8 + x / 8)
                    reg_attrib = @z80.memory.read8(reg_attrib_address)
                    ink = reg_attrib.byte_value & 7
                    paper = reg_attrib.byte_value & 38
                    flash = reg_attrib.bit?(7)
                    brightness = reg_attrib.bit?(6)
                    #TODO: use the colors from reg_attrib
                    7.times.each { |b| self.point(x + b, reg_y.byte_value) if reg_bitmap.bit?(b) }
                    reg_bitmap_addr.increase
                    x += 8
                end
                reg_y.increase
                reg_bitmap_addr.set_bit(5, reg_y.bit?(3))
                reg_bitmap_addr.set_bit(6, reg_y.bit?(4))
                reg_bitmap_addr.set_bit(7, reg_y.bit?(5))
                reg_bitmap_addr.set_bit(8, reg_y.bit?(0))
                reg_bitmap_addr.set_bit(9, reg_y.bit?(1))
                reg_bitmap_addr.set_bit(10, reg_y.bit?(2))
                reg_bitmap_addr.set_bit(11, reg_y.bit?(6))
                reg_bitmap_addr.set_bit(12, reg_y.bit?(7))
            end
        end
    end
end
