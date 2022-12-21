# frozen_string_literal: true

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
    MAX = [MAX0, MAX1, MAX2, MAX3, MAX4, MAX5, MAX6, MAX7]

    class Register8
        attr_reader :byte_value, :overflow, :hc, :carry

        def initialize
            @byte_value = 0
            @overflow, @hc, @carry = false
        end

        def value
            if @byte_value >= MAX7
                @value - MAX8
            else
                @byte_value
            end
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

        def bit?(b)
            @byte_value.to_s(2)[b] == 1
        end

        def set_bit(b)
            fail if b < 0 || b > 7
            @byte_value &= MAX[b]
        end

        def reset_bit(b)
            fail if b < 0 || b > 7
            @byte_value &= ~(MAX[b] + MAX8)
        end

        def negate
            @byte_value = ~(@byte_value + MAX8)
        end

        def shift_left
            if self.negative?
                @carry = true
                @byte_value = @byte_value << 1
            else
                @carry = false
                @byte_value = @byte_value << 1
            end
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

        def shift_right
            @carry = @byte_value.odd?
            @byte_value = @byte_value >> 1
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
            @byte_value, reg8.byte_value = reg8.byte_value, @byte_value
        end

        def copy reg8
            @byte_value = reg8.byte_value
        end

        def store(num)
            prev_value = @byte_value
            if num >= MAX7
                @byte_value = (MAX8 - 1) & num
                @overflow = true
            elsif num < -MAX7
                @byte_value = (MAX8 - 1) & -num
                @overflow = true
            elsif num.negative?
                @byte_value = MAX8 + num
            else
                @byte_value = num
            end
            @carry = @overflow
            @hc = ((prev_value < MAX4 && @byte_value >= MAX4) || (prev_value > MAX4 && @byte_value <= MAX4))
        end
    end

    class Flag8 < Register8
        attr_accessor :flag_c, :flag_n, :flag_pv, :flag_hc, :flag_z, :flag_s

        def initialize
            @flag_c, @flag_n, @flag_pv, @flag_hc, @flag_z, @flag_s = false
        end

        def value
            v = 0
            v += MAX0 if @flag_c
            v += MAX1 if @flag_n
            v += MAX2 if @flag_pv
            v += MAX4 if @flag_hc
            v += MAX6 if @flag_z
            v -= MAX8 if @flag_s
        end

        def store(num)
            v = num.to_s(2)
            @flag_c = (v[0] == '1')
            @flag_n = (v[1] == '1')
            @flag_pc = (v[2] == '1')
            @flag_hc = (v[4] == '1')
            @flag_z = (v[6] == '1')
            @flag_s = (v[8] == '1')
        end

        def parity reg
            @flag_pv = reg.value.to_s(2).count(1).even?
        end

        def s_z_p reg
            self.s_z(reg)
            self.parity(reg)
        end

        def s_z reg
            @flag_s = reg.negative?
            @flag_z = reg.value.zero?
        end

        def s_z_v_hc reg
            @flag_pv = reg.overflow
            @flag_hc = reg.hc
            self.s_z(reg)
        end

        def flags_shift reg
            @flag_n, @flag_hc = false
            @flag_c = reg.carry
        end

        def flags_math reg
            @flag_hc = reg.hc
            @flag_c = reg.overflow
        end
    end

    class Register16
        attr_reader :high, :low, :overflow, :hc, :carry

        def initialize h = Register8.new, l = Register8.new
            @high, @low = h, l
            @overflow, @hc, @carry = false
        end

        def value
            @high.value * MAX8 + @low.value
        end

        def copy reg16
            @high.value, @low.value = reg16.high.value, reg16.low.value
        end

        def exchange reg16
            low.exchange(reg16.low)
            high.exchange(reg16.high)
        end

        def store(num)
            if num >= MAX15
                num = MAX15 - num
                @overflow = true
            elsif num < -MAX15
                num = -MAX15 - num
                @overflow = true
            else
                @overflow = false
            end
            @carry = @overflow
            q, r = num.divmod MAX8
            @high.store(q)
            @low.store(r)
            @hc = @high.hc
        end
    end

    class Memory
        def initialize
            @memory = Array.new(49152) { Register8.new }
        end

        def read8 reg
            @memory[reg.value]
        end

        def read16 reg
            h = Register8.new
            h.store(reg.value + 1)
            Register16.new(@memory[h.value], @memory[reg.value])
        end
    end

    class Z80
        def initialize
            @a, @b, @c, @d, @e, @h, @l, @i, @r = [Register8.new] * 8
            @a’, @b’, @c’, @d’, @e’, @h’, @l’ = [Register8.new] * 8
            @f, @f’ = [Flag8.new] * 2
            @bc = Register16.new(@b, @c)
            @de = Register16.new(@d, @e)
            @hl = Register16.new(@h, @l)
            @af = Register16.new(@a, @f)
            @pc, @sp = [Register16.new] * 2
            @x = @y = 0
            @memory = Memory.new
            @state_duration, @t_states = 1, 4
            @iff1, @iff2, @can_execute = false, false, true
            @mode = 0
            @address_bus = Register16.new
            @data_bus = Register8.new
            @nonmaskable_interrupt_flag, @maskable_interrupt_flag = false
        end

        def memory_refresh
            val = @r.value + 1
            val = 0 if val == MAX7
            @r.store(val)
        end

        def next8
            val = @memory.read8(@pc)
            @pc.store(@pc.value + 1)
            memory_refresh
            val
        end

        def next16
            val = @memory.read16(@pc)
            @pc.store(@pc.value + 2)
            memory_refresh
            val
        end

        def push16
            @sp.store(@sp.value - 2)
            @memory.read16(@sp)
        end

        def pop16
            val = @memory.read16(@sp)
            @sp.store(@sp.value + 2)
            val
        end

        def read8indexed
            reg = Register16.new
            reg.store(@ix.value + self.next8)
            @memory.read8(reg)
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
                    execute self.next8
                end
                sleep(t + @t_states * @state_duration - Time.now) / 1000.0
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
                execute @data_bus
                @t_states += 2
            when 1
                @t_states = 13
                self.push16.copy(@pc)
                @pc.store(0x38)
            when 2
                @t_states = 19
                self.push16.copy(@pc)
                @pc.copy(@memory.read16(Register16.new(@i, @data_bus)))
            else
                fail
            end
        end

        def decode_register8 code, t = 3
            v = code & (MAX3 - 1)
            @t_states += t if v == 0x06
            [@b, @c, @d, @e, @h, @l, @memory.read8(@hl), @a][v]
        end

        def decode_register16 code
            [@bc, @de, @hl, @sp][code & 0x30]
        end

        def execute opcode
            @t_states = 4
            case opcode.value
            when 0x00 #NOP
            when 0x01 #LD BC,HHLL
                @t_states = 10
                @bc.copy(self.next16)
            when 0x02 #LD (BC),A
                @t_states = 7
                @memory[@bc.value].copy(@a)
            when 0x03 #INC BC
                @t_states = 6
                @bc.store(@bc.value + 1)
            when 0x04 #INC B
                @b.store(@b.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@b)
            when 0x05 #DEC B
                @b.store(@b.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@b)
            when 0x06 #LD B,NN
                @t_states = 7
                @b.copy(self.next8)
                op_size = 2
            when 0x07 #RLCA
                @a.rotate_left
                @f.flags_shift(@a)
            when 0x08 #EX AF,AF’
                @a.exchange(@a’)
                @f.exchange(@f’)
            when 0x09 #ADD HL,BC
                @t_states = 11
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @f.flags_math(@hl)
            when 0x0A #LD A,(BC)
                @t_states = 7
                @a.copy(@memory.read8(@bc))
            when 0x0B #DEC BC
                @bc.store(@bc.value - 1)
                @t_states = 6
            when 0x0C #INC C
                @c.store(@c.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@c)
            when 0x0D #DEC C
                @c.store(@c.value - 1)
                @f.flag_n = false
                @f.s_z_v_hc(@c)
            when 0x0E #LD C,NN
                @t_states = 7
                @c.copy(self.next8)
            when 0x0F #RRCA
                @a.rotate_right
                @f.flags_shift(@a)
            when 0x10 #DJNZ NN
                val = self.next8.value
                @b.store(@b.value - 1)
                if @b.nonzero?
                    @pc.store(@pc.value + val)
                    @t_states = 13
                else
                    @t_states = 8
                end
            when 0x11 #LD DE,HHLL
                @t_states = 10
                @de.copy(self.next16)
            when 0x12 #LD (DE),A
                @t_states = 7
                @memory.read8(@de).copy(@a)
            when 0x13 #INC DE
                @t_states = 6
                @de.store(@de.value + 1)
            when 0x14 #INC D
                @d.store(@d.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@d)
            when 0x15 #DEC D
                @d.store(@d.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@d)
            when 0x16 #LD D,NN
                @t_states = 7
                @d.store(self.next8)
            when 0x17 #RLA
                @a.carry = @f.flag_c
                @a.rotate_left_trough_carry
                @f.flags_shift(@a)
            when 0x18 #JR NN
                @t_states = 12
                @pc.store(@pc.value + self.next8.value)
            when 0x19 #ADD HL,DE
                @t_states = 11
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @f.flags_math(@hl)
            when 0x1A #LD A,(DE)
                @t_states = 7
                @a.copy(@memory.read8(@de))
            when 0x1B #DEC DE
                @t_states = 6
                @de.store(@de.value - 1)
            when 0x1C #INC E
                @e.store(@e.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@e)
            when 0x1D #DEC E
                @e.store(@e.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@e)
            when 0x1E #LD E,NN
                @t_states = 7
                @e.copy(self.next8)
            when 0x1F #RRA
                @a.carry = @f.flag_c
                @a.rotate_right_trough_carry
                @f.flags_shift(@a)
            when 0x20 #JR NZ,NN
                val = self.next8.value
                if @f.flag_z
                    @t_states = 7
                else
                    @pc.store(@pc.value + val)
                    @t_states = 12
                end
            when 0x21 #LD HL,HHLL
                @t_states = 10
                @hl.copy(self.next16)
            when 0x22 #LD (HHLL),HL
                @t_states = 16
                @memory.read16(self.next16).copy(@hl)
            when 0x23 #INC HL
                @t_states = 6
                @hl.store(@hl.value + 1)
            when 0x24 #INC H
                @h.store(@h.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@h)
            when 0x25 #DEC H
                @h.store(@h.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@h)
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
                val = self.next8.value
                if @f.flag_z
                    @pc.store(@pc.value + val)
                    @t_states = 12
                else
                    @t_states = 7
                end
            when 0x29 #ADD HL,HL
                @t_states = 11
                @hl.store(@hl.value + @hl.value)
                @f.flag_n = false
                @f.flags_math(@hl)
            when 0x2A #LD HL,(HHLL)
                @t_states = 16
                @hl.copy(@memory.read16(self.next16))
            when 0x2B #DEC HL
                @t_states = 6
                @hl.store(@hl.value - 1)
            when 0x2C #INC L
                @l.store(@l.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@l)
            when 0x2D #DEC L
                @l.store(@l.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@l)
            when 0x2E #LD L,NN
                @t_states = 7
                @l.copy(self.next8)
            when 0x2F #CPL
                @a.negate
                @f.flag_n, @f.flag_hc = true
            when 0x30 #JR NC,NN
                val = self.next8.value
                if @f.flag_c
                    @t_states = 7
                else
                    @pc.store(@pc.value + val)
                    @t_states = 12
                end
            when 0x31 #LD SP,HHLL
                @sp.copy(self.next16)
                @t_states = 10
            when 0x32 #LD (HHLL),A
                @t_states = 16
                @memory.read8(self.next16).copy(@a)
            when 0x33 #INC SP
                @t_states = 6
                @sp.store(@sp.value + 1)
            when 0x34 #INC (HL)
                @t_states = 11
                m = @memory.read8(@hl)
                m.store(m.value + 1)
                @f.flag_n = true
                @f.s_z_v_hc(m)
            when 0x35 #DEC (HL)
                @t_states = 11
                m = @memory.read8(@hl)
                m.store(m.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(m)
            when 0x36 #LD (HL),NN
                @t_states = 10
                @memory.read8(@hl).copy(self.next8)
            when 0x37 #SCF
                @f.flag_c = true
                @f.flag_n, @f.flag_hc = false
            when 0x38 #JR C,NN
                val = self.next8.value
                if @f.flag_c
                    @pc.store(@pc.value + val)
                    @t_states = 12
                else
                    @t_states = 7
                end
            when 0x39 #ADD HL,SP
                @t_states = 11
                @hl.store(@hl.value + @sp.value)
                @f.flag_n = false
                @f.flags_math(@hl)
            when 0x3A #LD A,(HHLL)
                @t_states = 13
                @a.copy(@memory.read8(self.next16))
            when 0x3B #DEC SP
                @t_states = 6
                @sp.store(@sp.value - 1)
            when 0x3C #INC A
                @a.store(@a.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@a)
            when  0x3D #DEC A
                @a.store(@a.value - 1)
                @f.flag_n = false
                @f.s_z_v_hc(@a)
            when 0x3E #LD A,NN
                @t_states = 7
                @a.copy(self.next8)
            when 0x3F #CCF
                @f.flag_hc = @f.flag_c
                @f.flag_c = !@f.flag_c
                @f.flag_n = false
            when 0x40 #LD B,B
            when 0x41 #LD B,C
                @b.copy(@c)
            when 0x42 #LD B,D
                @b.copy(@d)
            when 0x43 #LD B,E
                @b.copy(@e)
            when 0x44 #LD B,H
                @b.copy(@h)
            when 0x45 #LD B,L
                @b.copy(@l)
            when 0x46 #LD B,(HL)
                @t_states = 7
                @b.copy(@memory.read8(@hl))
            when 0x47 #LD B,A
                @b.copy(@a)
            when 0x48 #LD C,B
                @c.copy(@b)
            when 0x49 #LD C,C
            when 0x4A #LD C,D
                @c.copy(@d)
            when 0x4B #LD C,E
                @c.copy(@e)
            when 0x4C #LD C,H
                @c.copy(@h)
            when 0x4D #LD C,L
                @c.copy(@l)
            when 0x4E #LD C,(HL)
                @t_states = 7
                @c.copy(@memory.read8(@hl))
            when 0x4F #LD C,A
                @c.copy(@a)
            when 0x50 #LD D,B
                @d.copy(@b)
            when 0x51 #LD D,C
                @d.copy(@c)
            when 0x52 #LD D,D
            when 0x53 #LD D,E
                @d.copy(@e)
            when 0x54 #LD D,H
                @d.copy(@h)
            when 0x55 #LD D,L
                @d.copy(@l)
            when 0x56 #LD D,(HL)
                @t_states = 7
                @d.copy(@memory.read8(@hl))
            when 0x57 #LD D,A
                @d.copy(@a)
            when 0x58 #LD E,B
                @e.copy(@b)
            when 0x59 #LD E,C
                @e.copy(@c)
            when 0x5A #LD E,D
                @e.copy(@d)
            when 0x5B #LD E,E
            when 0x5C #LD E,H
                @e.copy(@h)
            when 0x5D #LD E,L
                @e.copy(@l)
            when 0x5E #LD E,(HL)
                @t_states = 7
                @e.copy(@memory.read8(@hl))
            when 0x5F #LD E,A
                @e.copy(@a)
            when 0x60 #LD H,B
                @h.copy(@b)
            when 0x61 #LD H,C
                @h.copy(@c)
            when 0x62 #LD H,D
                @h.copy(@d)
            when 0x63 #LD H,E
                @h.copy(@e)
            when 0x64 #LD H,H
            when 0x65 #LD H,L
                @h.copy(@l)
            when 0x66 #LD H,(HL)
                @t_states = 7
                @h.copy(@memory.read8(@hl))
            when 0x67 #LD H,A
                @h.copy(@a)
            when 0x68 #LD L,B
                @l.copy(@b)
            when 0x69 #LD L,C
                @l.copy(@c)
            when 0x6A #LD L,D
                @l.copy(@d)
            when 0x6B #LD L,E
                @l.copy(@e)
            when 0x6C #LD L,H
                @l.copy(@h)
            when 0x6D #LD L,L
            when 0x6E #LD L,(HL)
                @t_states = 7
                @l.copy(@memory.read8(@hl))
            when 0x6F #LD L,A
                @l.copy(@a)
            when 0x70 #LD (HL),B
                @t_states = 7
                @memory.read8(@hl).copy(@b)
            when 0x71 #LD (HL),C
                @t_states = 7
                @memory.read8(@hl).copy(@c)
            when 0x72 #LD (HL),D
                @t_states = 7
                @memory.read8(@hl).copy(@d)
            when 0x73 #LD (HL),E
                @t_states = 7
                @memory.read8(@hl).copy(@e)
            when 0x74 #LD (HL),H
                @t_states = 7
                @memory.read8(@hl).copy(@h)
            when 0x75 #LD (HL),L
                @t_states = 7
                @memory.read8(@hl).copy(@l)
            when 0x76 #HALT
                @can_execute = false
            when 0x77 #LD (HL),A
                @memory.read8(@hl).copy(@a)
                @t_states = 7
            when 0x78 #LD A,B
                @a.copy(@b)
            when 0x79 #LD A,C
                @a.copy(@c)
            when 0x7A #LD A,D
                @a.copy(@d)
            when 0x7B #LD A,E
                @a.copy(@e)
            when 0x7C #LD A,H
                @a.copy(@h)
            when 0x7D #LD A,L
                @a.copy(@l)
            when 0x7E #LD A,(HL)
                @t_states = 7
                @a.copy(@memory.read8(@hl))
            when 0x7F #LD A,A
            when 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87 #ADD A,r
                @a.store(@a.value + decode_register8(opcode).value)
                @f.flag_n, @f.flag_c = false, @a.carry
                @f.s_z_v_hc(@a)
            when 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F #ADC A,r
                @a.store(@a.value + decode_register8(opcode).value + (@f.flag_c ? 1 : 0))
                @f.flag_n, @f.flag_c = false, @a.carry
                @f.s_z_v_hc(@a)
            when 0x90, 0x91, 0x92, 0x93, 0x94, 0x94, 0x96, 0x97 #SUB A,r
                @a.store(@a.value - decode_register8(opcode).value)
                @f.flag_n, @f.flag_c = true, @a.carry
                @f.s_z_v_hc(@a)
            when 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F #SBC A,r
                @a.store(@a.value - decode_register8(opcode).value - (@f.flag_c ? 1 : 0))
                @f.flag_n, @f.flag_c = true, @a.carry
                @f.s_z_v_hc(@a)
            when 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7 #AND A,r
                @a.store(@a.value & decode_register8(opcode).value)
                @f.s_z(@a)
                @f.flag_pv, @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false, true
            when 0xA8, 0xA9,0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF #XOR A,r
                @a.store(@a.value ^ decode_register8(opcode).value)
                @f.s_z_p(@a)
                @f.flag_hc, @f.flag_n, @f.flag_c = false
            when 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7 #OR A,r
                @a.store(@a.value | decode_register8(opcode).value)
                @f.s_z(@a)
                @f.flag_pv, @f.flag_hc, @f.flag_n, @f.flag_c = false
            when 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF #CP A,r
                @f.flag_z = (@a.value == decode_register8(opcode).value)
            when 0xC0 #RET NZ
                if @f.flag_z
                    @t_states = 11
                else
                    @pc.copy(self.pop16)
                    @t_states = 15
                end
            when 0xC1 #POP BC
                @t_states = 10
                @bc.copy(self.pop16)
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
            when 0xC5 #PUSH BC
                @t_states = 10
                self.push16.copy(@bc)
            when 0xC6 #ADD A,NN
                @t_states = 7
                @a.store(@a.value + self.next8.value)
                @f.flag_n, @f.flag_c = false, @a.carry
                @f.s_z_v_hc(@a)
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
                opcode = self.next8.value
                case opcode
                when 0x00..0x3F
                    reg = decode_register8(opcode, 7)
                    case opcode
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
                    @f.flags_shift(reg)
                    @f.s_z_p(reg)
                when 0x40..0x7F #BIT b,r
                    @f.flag_z = !(decode_register8(opcode).bit?(opcode & 0x38))
                    @f.flag_hc, @f.flag_n = true, false
                when 0x80..0xBF #RES b,r
                    decode_register8(opcode, 7).reset_bit(opcode & 0x38)
                when 0xC0..0xFF #SET b,r
                    decode_register8(opcode, 7).set_bit(opcode & 0x38)
                else
                    fail
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
                @a.add(self.next8.value + (@f.carry ? 1 : 0), @f)
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
            when 0xD1 #POP DE
                @t_states = 10
                @de.copy(self.pop16)
            when 0xD2 #JP NC,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if !@f.flag_c
            when 0xD3 #OUT (NN),A
                @t_states = 11
                @address_bus = Register16.new(@a, self.next8)
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
            when 0xD5 #PUSH DE
                @t_states = 10
                self.push16.copy(@de)
            when 0xD6 #SUB A,NN
                @t_states = 7
                @a.store(@a.value - self.next8.value)
                @f.flag_n, @f.flag_c = true, @a.carry
                @f.s_z_v_hc(@a)
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
                @address_bus = Register16.new(@a, self.next8)
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
                opcode = self.next8.value
                case opcode
                when 0x09, 0x19, 0x29, 0x39 #ADD IX,pp
                    @t_states = 15
                    @ix.store(@ix.value + [@bc, @de, @ix, @sp][opcode & 0x30].value)
                    @f.flags_math(@ix)
                    @f.flag_n = false
                when 0x21 #LD IX,nn
                    @t_states = 14
                    @ix.copy(self.next16)
                when 0x22 #LD (nn),IX
                    @t_states = 20
                    @memory.read16(self.next16).copy(@ix)
                when 0x23 #INC IX
                    @t_states = 10
                    @ix.store(@ix.value + 1)
                when 0x2A #LD IX,(nn)
                    @t_states = 20
                    @ix.copy(@memory.read16(self.next16))
                when 0x2B #DEC IX 
                    @t_states = 10
                    @ix.store(@ix.value - 1)
                when 0x34 #INC (IX+d)
                    @t_states = 23
                    reg = self.read8indexed
                    reg.store(reg.value + 1)
                    @f.s_z_v_hc(reg)
                    @f.flag_n = false
                when 0x35 #DEC (IX+d)
                    @t_states = 23
                    reg = self.read8indexed
                    reg.store(reg.value - 1)
                    @f.s_z_v_hc(reg)
                    @f.flag_n = true
                when 0x36 #LD (IX+d),n
                    @t_states = 19
                    self.read8indexed.store(self.next8)
                when 0x46, 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x7E #LD r,(IX+d)
                    @t_states = 19
                    [@b, @c, @d, @e, @h, @l, nil, @a][opcode & 0x38].copy(self.read8indexed)
                when 0x86, 0x8E #ADD/ADC A,(IX+d)
                    @t_states = 19
                    @a.store(@a.value + self.read8indexed + (opcode == 0x8E && @f.flag_c ? 1 : 0))
                    @f.s_z_v_hc(@a)
                    @f.flag_n = false
                when 0x96, 0x9E #SUB/SBC A,(IX+d)
                    @t_states = 19
                    @a.store(@a.value - self.read8indexed - (opcode == 0x9E && @f.flag_c ? 1 : 0))
                    @f.s_z_v_hc(@a)
                    @f.flag_n = true
                when 0xA6 #AND A,(IX+d)
                    @t_states = 19
                    @a.store(@a.value & self.read8indexed)
                    @f.s_z(@a)
                    @f.flag_pv, @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false, true
                when 0xAE #XOR A,(IX+d)
                    @t_states = 19
                    @a.store(@a.value ^ self.read8indexed)
                    @f.s_z_p(@a)
                    @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false
                when 0xB6 #OR A,(IX+d)
                    @t_states = 19
                    @a.store(@a.value | self.read8indexed)
                    @f.s_z(@a)
                    @f.flag_pv, @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false, false
                when 0xBE #CP A,(IX+d)
                    @t_states = 19
                    @f.flag_z = (@a.value == self.read8indexed.value)
                when 0xCB #DDCB
                    opcode = self.next8.value
                    reg = self.read8indexed
                    case opcode
                    when 0x06 #RLC (IX+d)	
                        @t_states = 23
                        reg.rotate_left
                        @f.s_z_p(@reg)
                        @f.flags_shift(@reg)
                    when 0x0E #RRC (IX+d)
                        @t_states = 23
                        reg.rotate_right
                        @f.s_z_p(@reg)
                        @f.flags_shift(@reg)
                    when 0x16 #RL (IX+d)
                        @t_states = 23
                        reg.rotate_left_trough_carry
                        @f.s_z_p(@reg)
                        @f.flags_shift(@reg)
                    when 0x1E #RR (IX+d)
                        @t_states = 23
                        reg.rotate_right_trough_carry
                        @f.s_z_p(@reg)
                        @f.flags_shift(@reg)
                    when 0x26 #SLA (IX+d)
                        @t_states = 23
                        reg.shift_left
                        @f.s_z_p(reg)
                        @f.flags_shift(reg)
                    when 0x2E #SRA (IX+d)
                        @t_states = 23
                        reg.carry = reg.negative?
                        reg.rotate_right_trough_carry                    
                        @f.s_z_p(reg)
                        @f.flags_shift(reg)
                    when 0x36 #SLL (IX+d)
                        @t_states = 23
                        reg.carry = true
                        reg.rotate_left_trough_carry                    
                        @f.s_z_p(reg)
                        @f.flags_shift(reg)
                    when 0x3E #SRL (IX+d)
                        @t_states = 23
                        reg.carry = false
                        reg.rotate_right_trough_carry                    
                        @f.s_z_p(reg)
                        @f.flags_shift(reg)
                    when 0x46, 0x4E, 0x56, 0x5E, 0x66, 0x6E, 0x76, 0x7E #BIT b,(IX+d)
                        @t_states = 20
                        @f.flag_z = !(reg.bit?(opcode & 0x38))
                        @f.flag_hc, @f.flag_n = true, false
                    when 0x86, 0x8E, 0x96, 0x9E, 0xA6, 0xAE, 0xB6, 0xBE #RES b,(IX+d)
                        @t_states = 23
                        reg.reset_bit(opcode & 0x38)
                    when 0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE #SET b,(IX+d)
                        @t_states = 20
                        reg.set_bit(opcode & 0x38)
                    else
                        fail
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
                    @pc.copy(@memory.read16(@ix))
                when 0xF9 #LD SP,IX
                    @t_states = 10
                    @sp.copy(@ix)
                else
                    fail
                end
            when 0xDE #SBC A,NN
                @t_states = 7
                @a.sub(self.next8 + (@f.carry ? 1 : 0), @f)
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
            when 0xE1 #POP HL
                @t_states = 10
                @hl.copy(self.pop16)
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
            when 0xE5 #PUSH HL
                @t_states = 10
                self.push16.copy(@hl)
            when 0xE6 #AND A,NN
                @t_states = 7
                @a.store(@a.value & self.next8.value)
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
                #TODO: ED
                opcode = self.next8.value
                case opcode
                when 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x78 #IN r,(C)
                    @t_states = 12
                    reg = [@b, @c, @d, @e, @h, @l, nil, @a][opcode & 0x38]
                    @address_bus.copy(@bc)
                    #TODO: read one byte from device in address_bus to data_bus
                    reg.copy(@data_bus)
                    @f.s_z_p(reg)
                    @f.flag_hc, @f.flag_n = false, false
                when 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x79 #OUT (C),r
                    @t_states = 12
                    reg = [@b, @c, @d, @e, @h, @l, nil, @a][opcode & 0x38]
                    @address_bus.copy(@bc)
                    @data_bus.copy(reg)
                    #TODO: write one byte from data_bus to device in address_bus
                when 0x42, 0x52, 0x62, 0x72 #SBC HL,ss
                    @t_states = 15
                    @hl.store(@hl.value - decode_register16(opcode).value - (@f.flag_c ? 1 : 0))
                    @f.s_z_v_hc(@hl)
                    @f.flag_n, @f.flag_c = false, @hl.carry
                when 0x43, 0x53, 0x63, 0x73 #LD (nn),dd
                    @t_states = 20
                    @memory.read16(self.next16).copy(decode_register16(opcode))
                when 0x44 #NEG
                    @t_states = 8
                    @a.store(-@a.value)
                    @f.s_z_v_hc(@a)
                    @f.flag_c, @f.flag_n = @a.value.nonzero?, true
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
                    @hl.store(@hl.value + decode_register16(opcode).value + (@f.flag_c ? 1 : 0))
                    @f.s_z_v_hc(@hl)
                    @f.flags_math(@hl)
                    @f.flag_n = false
                when 0x4B, 0x5B, 0x6B, 0x7B #LD dd,(nn)
                    @t_states = 20
                    @memory.read16(decode_register16(opcode)).store(self.next16)
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
                else
                    fail
                end
            when 0xEE #XOR A,NN
                @t_states = 7
                @a.store(@a.value ^ self.next8.value)
                @f.s_z_p(@a)
                @f.flag_hc, @f.flag_n, @f.flag_c = false
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
            when 0xF1 #POP AF
                @t_states = 10
                @af.copy(self.pop16)
            when 0xF2 #JP P,HHLL
                @t_states = 10
                reg = self.next16
                @pc.copy(reg) if !@f.flag_s
            when 0xF3 #DI
                @iff1, @iff2 = false
            when 0xF4 #CALL P,HHLL
                reg = self.next16
                if @f.flag_s
                    @t_states = 10
                else
                    @t_states = 17
                    self.push16.copy(@pc)
                    @pc.copy(reg)
                end
            when 0xF5 #PUSH AF
                @t_states = 10
                self.push16.copy(@af)
            when 0xF6 #OR A,NN
                @t_states = 7
                @a.store(@a.value | self.next8.value)
                @f.s_z(@a)
                @f.flag_pv, @f.flag_hc, @f.flag_n, @f.flag_c = false
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
            when 0xDD #FD
                #TODO: FD
                fail
            when 0xFE #CP A,NN
                @f.flag_z = (@a.value == self.next8.value)
            when 0xFF #RST 38
                @t_states = 11
                self.push16.copy(@pc)
                @pc.store(0x38)
            else
                fail
            end
        end
    end

end

#TODO: sp, pc, ix, iy are unsigned ?
#TODO: i is part of ix ?
#TODO: what happens if an undefined opcode is found ?
#TODO: how to set carry and hc (for example on ADD A,A) ??
#TODO: compact LD, INC, DEC, POP, PUSH opcodes using decode_register
z80 = Z80::Z80.new
#z80.run
