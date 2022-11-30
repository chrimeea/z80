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
            val = @byte_value & (MAX3 - 1)
            return @byte_value - val, val
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

        def set_sign_bit
            @byte_value += MAX7 if @byte_value < MAX7
        end

        def reset_sign_bit
            @byte_value -= MAX7 if @byte_value >= MAX7
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
            self.set_sign_bit if @carry
        end

        def rotate_right_trough_carry
            v = @carry
            self.shift_right
            self.set_sign_bit if @carry
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
        attr_reader :high, :low, :overflow, :hc

        def initialize h, l
            @high, @low = h, l
            @overflow, @hc = false
        end

        def value
            @high.value * MAX8 + @low.value
        end

        def copy reg16
            @high.value, @low.value = reg16.high.value, reg16.low.value
        end

        def store(num)
            prev_high = @high
            if num >= MAX15
                num = MAX15 - num
                @overflow = true
            elsif num < -MAX15
                num = -MAX15 - num
                @overflow = true
            else
                @overflow = false
            end
            q, r = num.divmod MAX8
            self.store q, r
            @hc = ((prev_high.abs < MAX4 && @high.abs >= MAX4) || (prev_high.abs > MAX4 && @high.abs <= MAX4))
        end
    end

    class Register16U
        attr_reader :value, :overflow

        def initialize
            @value, @overflow = 0, false
        end

        def store(num)
            if num >= MAX16
                @value, @overflow = num - MAX16, true
            else
                @value, @overflow = num, false
            end
        end

        def read8 mem
            val = mem[@value]
            self.store(@value + 1)
            val
        end

        def read16 mem
            self.store(@value + 2)
            Register16.new(mem[@value - 1], mem[@value - 2])
        end

        def push mem
            self.store(@value - 2)
            Register16.new(mem[@value + 1], mem[@value])
        end
    end

    class Z80
        def initialize
            @a, @b, @c, @d, @e, @h, @l = [Register8.new] * 8
            @a’, @b’, @c’, @d’, @e’, @h’, @l’ = [Register8.new] * 8
            @f, @f’ = [Flag8.new] * 2
            @bc = Register16.new(@b, @c)
            @de = Register16.new(@d, @e)
            @hl = Register16.new(@h, @l)
            @af = Register16.new(@a, @f)
            @pc, @sp = [Register16U.new] * 2
            @i = 0
            @x = @y = 0
            @memory = Array.new(49152) { Register8.new }
            @state_duration, @t_states = 1, 4
            @can_interrupt, @can_execute = true
        end

        def run
            loop do
                interrupt if can_interrupt
                t = Time.now
                @t_states = 4
                execute(@pc.read8(@memory).value) if can_execute
                sleep(t + @t_states * @state_duration - Time.now) / 1000.0
            end
        end

        def interrupt
        end

        def decode_register code, t = 3
            case code & (MAX3 - 1)
            when 0x00
                @b
            when 0x01
                @c
            when 0x02
                @d
            when 0x03
                @e
            when 0x04
                @h
            when 0x05
                @l
            when 0x06
                @t_states += t
                @memory[@hl.value]
            when 0x07
                @a
            else
                fail
            end
        end

        def execute opcode
            case opcode
            when 0x00 #NOP
            when 0x01 #LD BC,HHLL
                @bc.copy(@pc.read16(@memory))
                @t_states = 10
                op_size = 3
            when 0x02 #LD (BC),A
                @memory[@bc.value].copy(@a)
                @t_states = 7
            when 0x03 #INC BC
                @bc.store(@bc.value + 1)
                @t_states = 6
            when 0x04 #INC B
                @b.store(@b.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@b)
            when 0x05 #DEC B
                @b.store(@b.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@b)
            when 0x06 #LD B,NN
                @b.copy(@pc.read8(@memory))
                @t_states = 7
                op_size = 2
            when 0x07 #RLCA
                @a.rotate_left
                @f.flags_shift(@a)
            when 0x08 #EX AF,AF’
                @a.exchange(@a’)
                @f.exchange(@f’)
            when 0x09 #ADD HL,BC
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @f.flags_math(@hl)
                @t_states = 11
            when 0x0A #LD A,(BC)
                @a.copy(@memory[@bc.value])
                @t_states = 7
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
                @c.copy(@pc.read8(@memory))
                @t_states = 7
            when 0x0F #RRCA
                @a.rotate_right
                @f.flags_shift(@a)
            when 0x10 #DJNZ NN
                val = @pc.read8(@memory).value
                @b.store(@b.value - 1)
                if @b.nonzero?
                    @pc.store(@pc.value + val)
                    @t_states = 13
                else
                    @t_states = 8
                end
            when 0x11 #LD DE,HHLL
                @de.copy(@pc.read16(@memory))
                @t_states = 10
            when 0x12 #LD (DE),A
                @memory[@de.value].copy(@a)
                @t_states = 7
            when 0x13 #INC DE
                @de.store(@de.value + 1)
                @t_states = 6
            when 0x14 #INC D
                @d.store(@d.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@d)
            when 0x15 #DEC D
                @d.store(@d.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@d)
            when 0x16 #LD D,NN
                @d.store(@pc.read8(@memor))
                @t_states = 7
            when 0x17 #RLA
                @a.carry = @f.flag_c
                @a.rotate_left_trough_carry
                @f.flags_shift(@a)
            when 0x18 #JR NN
                @pc.store(@pc.value + @pc.read8(@memory).value)
                @t_states = 12
            when 0x19 #ADD HL,DE
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @f.flags_math(@hl)
                @t_states = 11
            when 0x1A #LD A,(DE)
                @a.copy(@memory[@de.value])
                @t_states = 7
            when 0x1B #DEC DE
                @de.store(@de.value - 1)
                @t_states = 6
            when 0x1C #INC E
                @e.store(@e.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@e)
            when 0x1D #DEC E
                @e.store(@e.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@e)
            when 0x1E #LD E,NN
                @e.copy(@pc.read8(@memory))
                @t_states = 7
            when 0x1F #RRA
                @a.carry = @f.flag_c
                @a.rotate_right_trough_carry
                @f.flags_shift(@a)
            when 0x20 #JR NZ,NN
                val = @pc.read8(@memory).value
                if @f.flag_z
                    @t_states = 7
                else
                    @pc.store(@pc.value + val)
                    @t_states = 12
                end
            when 0x21 #LD HL,HHLL
                @hl.copy(@pc.read16(@memory))
                @t_states = 10
            when 0x22 #LD (HHLL),HL
                v = @pc.read16(@memory).value
                Register16.new(@memory[v + 1], @memory[v]).copy(@hl)
                @t_states = 16
            when 0x23 #INC HL
                @hl.store(@hl.value + 1)
                @t_states = 6
            when 0x24 #INC H
                @h.store(@h.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@h)
            when 0x25 #DEC H
                @h.store(@h.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@h)
            when 0x26 #LD H,NN
                @h.copy(@pc.read8(@memory))
                @t_states = 7
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
                val = @pc.read8(@memory).value
                if @f.flag_z
                    @pc.store(@pc.value + val)
                    @t_states = 12
                else
                    @t_states = 7
                end
            when 0x29 #ADD HL,HL
                @hl.store(@hl.value + @hl.value)
                @f.flag_n = false
                @f.flags_math(@hl)
                @t_states = 11
            when 0x2A #LD HL,(HHLL)
                v = @pc.read16(@memory).value
                @hl.copy(Register16.new(@memory[v + 1].value, @memory[v].value))
                @t_states = 16
            when 0x2B #DEC HL
                @hl.store(@hl.value - 1)
                @t_states = 6
            when 0x2C #INC L
                @l.store(@l.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@l)
            when 0x2D #DEC L
                @l.store(@l.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(@l)
            when 0x2E #LD L,NN
                @l.copy(@pc.read8(@memory))
                @t_states = 7
            when 0x2F #CPL
                @a.negate
                @f.flag_n, @f.flag_hc = true
            when 0x30 #JR NC,NN
                val = @pc.read8(@memory).value
                if @f.flag_c
                    @t_states = 7
                else
                    @pc.store(@pc.value + val)
                    @t_states = 12
                end
            when 0x31 #LD SP,HHLL
                @sp.copy(@pc.read16(@memory))
                @t_states = 10
            when 0x32 #LD (HHLL),A
                @memory[@pc.read16(@memory).value].copy(@a)
                @t_states = 16
            when 0x33 #INC SP
                @sp.store(@sp.value + 1)
                @t_states = 6
            when 0x34 #INC (HL)
                m = @memory[@hl.value]
                m.store(m.value + 1)
                @f.flag_n = true
                @f.s_z_v_hc(m)
                @t_states = 11
            when 0x35 #DEC (HL)
                m = @memory[@hl.value]
                m.store(m.value - 1)
                @f.flag_n = true
                @f.s_z_v_hc(m)
                @t_states = 11
            when 0x36 #LD (HL),NN
                @memory[@hl.value].copy(@pc.read8(@memory))
                @t_states = 10
            when 0x37 #SCF
                @f.flag_c = true
                @f.flag_n, @f.flag_hc = false
            when 0x38 #JR C,NN
                val = @pc.read8(@memory).value
                if @f.flag_c
                    @pc.store(@pc.value + val)
                    @t_states = 12
                else
                    @t_states = 7
                end
            when 0x39 #ADD HL,SP
                @hl.store(@hl.value + @sp.value)
                @f.flag_n = false
                @f.flags_math(@hl)
                @t_states = 11
            when 0x3A #LD A,(HHLL)
                @a.copy(@memory[@pc.read16(@memory).value])
                @t_states = 13
            when 0x3B #DEC SP
                @sp.store(@sp.value - 1)
                @t_states = 6
            when 0x3C #INC A
                @a.store(@a.value + 1)
                @f.flag_n = false
                @f.s_z_v_hc(@a)
            when  0x3D #DEC A
                @a.store(@a.value - 1)
                @f.flag_n = false
                @f.s_z_v_hc(@a)
            when 0x3E #LD A,NN
                @a.copy(@pc.read8(@memory))
                @t_states = 7
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
                @b.copy(@memory[@hl.value])
                @t_states = 7
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
                @c.copy(@memory[@hl.value])
                @t_states = 7
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
                @d.copy(@memory[@hl.value])
                @t_states = 7
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
                @e.copy(@memory[@hl.value])
                @t_states = 7
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
                @h.copy(@memory[@hl.value])
                @t_states = 7
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
                @l.copy(@memory[@hl.value])
                @t_states = 7
            when 0x6F #LD L,A
                @l.copy(@a)
            when 0x70 #LD (HL),B
                @memory[@hl.value].copy(@b)
                @t_states = 7
            when 0x71 #LD (HL),C
                @memory[@hl.value].copy(@c)
                @t_states = 7
            when 0x72 #LD (HL),D
                @memory[@hl.value].copy(@d)
                @t_states = 7
            when 0x73 #LD (HL),E
                @memory[@hl.value].copy(@e)
                @t_states = 7
            when 0x74 #LD (HL),H
                @memory[@hl.value].copy(@h)
                @t_states = 7
            when 0x75 #LD (HL),L
                @memory[@hl.value].copy(@l)
                @t_states = 7
            when 0x76 #HALT
                @can_execute = false
            when 0x77 #LD (HL),A
                @memory[@hl.value].copy(@a)
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
                @a.copy(@memory[@hl.value])
                @t_states = 7
            when 0x7F #LD A,A
            when 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87 #ADD A,r
                @a.store(@a.value + decode_register(opcode).value)
                @f.flag_n, @f.flag_c = false, @a.carry
                @f.s_z_v_hc(@a)
            when 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F #ADC A,r
                @a.store(@a.value + decode_register(opcode).value + (@f.flag_c ? 1 : 0))
                @f.flag_n, @f.flag_c = false, @a.carry
                @f.s_z_v_hc(@a)
            when 0x90, 0x91, 0x92, 0x93, 0x94, 0x94, 0x96, 0x97 #SUB A,r
                @a.store(@a.value - decode_register(opcode).value)
                @f.flag_n, @f.flag_c = true, @a.carry
                @f.s_z_v_hc(@a)
            when 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F #SBC A,r
                @a.store(@a.value - decode_register(opcode).value - (@f.flag_c ? 1 : 0))
                @f.flag_n, @f.flag_c = true, @a.carry
                @f.s_z_v_hc(@a)
            when 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7 #AND A,r
                @a.store(@a.value & decode_register(opcode).value)
                @f.s_z(@a)
                @f.flag_pv, @f.flag_n, @f.flag_c, @f.flag_hc = false, false, false, true
            when 0xA8, 0xA9,0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF #XOR A,r
                @a.store(@a.value ^ decode_register(opcode).value)
                @f.s_z_p(@a)
                @f.flag_hc, @f.flag_n, @f.flag_c = false
            when 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7 #OR A,r
                @a.store(@a.value | decode_register(opcode).value)
                @f.s_z(@a)
                @f.flag_pv, @f.flag_hc, @f.flag_n, @f.flag_c = false
            when 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF #CP A,r
                @f.flag_z = (@a.value == decode_register(opcode).value)
            when 0xC0 #RET NZ
                if @f.flag_z
                    @t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15
                end
            when 0xC1 #POP BC
                @bc.copy(@sp.read16(@memory))
                @t_states = 10
            when 0xC2 #JP NZ,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_z
                @t_states = 10
            when 0xC3 #JP HHLL
                @pc.copy(@pc.read16(@memory))
                @t_states = 10
            when 0xC4 #CALL NZ,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_z
                    @t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                end
            when 0xC5 #PUSH BC
                @sp.push(@memory).copy(@bc)
                @t_states = 10
            when 0xC6 #ADD A,NN
                @a.add(@pc.read8(@memory).value, @f)
                @t_states = 7
            when 0xC7 #RST 00
                @sp.push(@memory).copy(@pc)
                @pc.copy(0)
                @t_states = 11
            when 0xC8 #RET Z
                if @f.flag_z
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15
                else
                    @t_states = 11
                end
            when 0xC9 #RET
                @pc.copy(@sp.read16(@memory))
                @t_states = 10
            when 0xCA #JP Z,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if @f.flag_z
                @t_states = 10
            when 0xCB #CB
                opcode = @pc.read8(@memory)
                case opcode
                when 0x00..0x3F
                    reg = decode_register(opcode, 7)
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
                    @f.flag_z = !(decode_register(opcode).bit?(opcode & 0x38))
                    @f.flag_hc, @f.flag_n = true, false
                when 0x80..BF #RES b,r
                    decode_register(opcode, 7).reset_bit(opcode & 0x38)
                when 0xC0..FF #SET b,r
                    decode_register(opcode, 7).set_bit(opcode & 0x38)
                else
                    fail
                end
                @t_states += 4
            when 0xCC #CALL Z,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_z
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                else
                    @t_states = 10
                end
            when 0xCD #CALL HHLL
                reg = @pc.read16(@memory)
                @sp.push(@memory).copy(@pc)
                @pc.copy(reg)
                @t_states = 17
            when 0xCE #ADC A,NN
                @a.add(@pc.read8(@memory).value + (@f.carry ? 1 : 0), @f)
                @t_states = 7
            when 0xCF #RST 08
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x08)
                @t_states = 11
            when 0xD0 #RET NC
                if @f.flag_c
                    @t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15
                end
            when 0xD1 #POP DE
                @de.copy(@sp.read16(@memory))
                @t_states = 10
            when 0xD2 #JP NC,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_c
                @t_states = 10
            when 0xD3 #OUT (NN),A
                #TODO: OUT
                fail
            when 0xD4 #CALL NC,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_c
                    @t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                end
            when 0xD5 #PUSH DE
                @sp.push(@memory).copy(@de)
                @t_states = 10
            when 0xD6 #SUB A,NN
                @a.sub(@pc.read8(@memory), @f)
                @t_states = 7
            when 0xD7 #RST 10
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x10)
                @t_states = 11
            when 0xD8 #RET C
                if @f.flag_c
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15                    
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
                reg = @pc.read16(@memory)
                @pc.copy(reg) if @f.flag_c
                @t_states = 10
            when 0xDB #IN A,(NN)
                #TODO: IN
                fail
            when 0xDC #CALL C,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_c
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                else
                    @t_states = 10
                end
            when 0xDD #DD
                #TODO: DD
                fail
            when 0xDE #SBC A,NN
                @a.sub(@pc.read8(@memory) + (@f.carry ? 1 : 0), @f)
                @t_states = 7
            when 0xDF #RST 18
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x18)
                @t_states = 11
            when 0xE0 #RET PO
                if @f.flag_pv
                    @t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15
                end
            when 0xE1 #POP HL
                @hl.copy(@sp.read16(@memory))
                @t_states = 10
            when 0xE2 #JP PO,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_pv
                @t_states = 10
            when 0xE3 #EX (SP),HL
                @l.exchange(@memory[@sp.value])
                @h.exchange(@memory[@sp.value + 1])
                @t_states = 19
            when 0xE4 #CALL PO,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_pv
                    @t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                end
            when 0xE5 #PUSH HL
                @sp.push(@memory).copy(@hl)
                @t_states = 10
            when 0xE6 #AND A,NN
                @a.and(@pc.read8(@memory), @f)
            when 0xE7 #RST 20
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x20)
                @t_states = 11
            when 0xE8 #RET PE
                if @f.flag_pv
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15
                else
                    @t_states = 11
                end
            when 0xE9 #JP (HL)
                @pc.copy(Register16.new(@memory[@hl.value + 1], @memory[@hl.value]))
            when 0xEA #JP PE,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if @f.flag_pv
                @t_states = 10
            when 0xEB #EX DE,HL
                @d.exchange(@h)
                @e.exchange(@l)
            when 0xEC #CALL PE,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_pv
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                else
                    @t_states = 10
                end
            when 0xED #ED
                #TODO: ED
                fail
            when 0xEE #XOR A,NN
                @a.xor(@pc.read8(@memory), @f)
                @t_states = 7
            when 0xEF #RST 28
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x28)
                @t_states = 11
            when 0xF0 #RET P
                if @f.flag_s
                    @t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15
                end
            when 0xF1 #POP AF
                @af.copy(@sp.read16(@memory))
                @t_states = 10
            when 0xF2 #JP P,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_s
                @t_states = 10
            when 0xF3 #DI
                can_interrupt = false
            when 0xF4 #CALL P,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_s
                    @t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                end
            when 0xF5 #PUSH AF
                @sp.push(@memory).copy(@af)
                @t_states = 10
            when 0xF6 #OR A,NN
                @a.or(@pc.read8(@memory), @f)
                @t_states = 7
            when 0xF7 #RST 30
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x30)
                @t_states = 11
            when 0xF8 #RET M
                if @f.flag_s
                    @pc.copy(@sp.read16(@memory))
                    @t_states = 15
                else
                    @t_states = 11
                end
            when 0xF9 #LD SP,HL
                @sp.copy(@hl)
            when 0xFA #JP M,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if @f.flag_s
                @t_states = 10
            when 0xFB #EI
                @can_interrupt = true
            when 0xFC #CALL M,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_s
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    @t_states = 17
                else
                    @t_states = 10
                end
            when 0xDD #FD
                #TODO: FD
                fail
            when 0xFE #CP A,NN
                @f.flag_z = (@a.value == @pc.read8(@memory).value)
            when 0xFF #RST 38
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x38)
                @t_states = 11
            else
                fail
            end
        end
    end

end

#TODO: how to set carry (for example on ADD A,A) ??
z80 = Z80::Z80.new
#z80.run
