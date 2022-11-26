# frozen_string_literal: true

module Z80

    MAX0 = 0x1
    MAX1 = 0x2
    MAX2 = 0x4
    MAX4 = 0x10
    MAX6 = 0x40
    MAX7 = 0x80
    MAX8 = 0x100
    MAX15 = 0x8000

    class Register8
        attr_reader :value, :overflow, :hc, :carry

        def initialize
            @value = 0
            @overflow, @hc, @carry = false
        end

        def as_unsigned
            if @value.negative?
                MAX8 - @value
            else
                @value
            end
        end

        def shift_left
            if @value.negative?
                @carry = true
                v = self.as_unsigned << 1
            else
                @carry = false
                v = @value << 1
            end
            if v >= MAX7
                v = MAX8 - v
            end
            @value = v
        end

        def rotate_left
            self.shift_left
            @value += 1 if @carry
        end

        def rotate_left_trough_carry c
            v, @carry = c
            self.shift_left
            @value += 1 if v
        end

        def shift_right
            @carry = @value.odd?
            if @value.negative?
                v = self.as_unsigned >> 1
            else
                v = @value >> 1
            end
            @value = v
        end

        def rotate_right
            self.shift_right
            @value -= MAX7 if @carry
        end

        def rotate_right_trough_carry c
            v, @carry = c
            self.shift_right
            @value -= MAX7 if v
        end

        def flags f
            f.flag_pv = @overflow
            f.flag_hc = @hc
            f.s_z(@value)
        end

        def exchange reg8
            @value, reg8.value = reg8.value, @value
        end

        def copy reg8
            @value = reg8.value
        end

        def store(num)
            prev_value = @value
            if num >= MAX7
                @value = MAX7 - num
                @overflow = true
            elsif num < -MAX7
                @value = -MAX7 - num
                @overflow = true
            else
                @value = num
                @overflow = false
            end
            @hc = ((prev_value.abs < MAX4 && @value.abs >= MAX4) || (prev_value.abs > MAX4 && @value.abs <= MAX4))
        end

        def add(num, f)
            self.store(@value + num)
            f.flag_n = false
            f.flag_c = @carry
            self.flags(f)
        end

        def sub(num, f)
            self.store(@value + num)
            f.flag_n = true
            f.flag_c = @carry
            self.flags(f)
        end

        def and(num, f)
            self.store(@value & num)
            f.s_z(@value)
            f.flag_hc = true
            f.flag_pv, f.flag_n, f.flag_c = false
        end

        def xor(num, f)
            self.store(@value ^ num)
            f.s_z(@value)
            f.parity(@value)
            f.flag_hc, f.flag_n, f.flag_c = false
        end

        def or(num, f)
            self.store(@value | num)
            f.s_z(@value)
            f.flag_pv, f.flag_hc, f.flag_n, f.flag_c = false
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

        def parity(val)
            @flag_pv = val.to_s(2).count(1).even?
        end

        def s_z(val)
            @flag_s = val.negative?
            @flag_z = val.zero?
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

        def flags_math f
            f.flag_hc = @hc
            f.flag_c = @overflow
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
            @state_duration = 1
            @can_interrupt, @can_execute = true
        end

        def run
            loop do
                interrupt if can_interrupt
                if can_execute
                    execute(@pc.read8(@memory).value)
                else
                    execute(0x00)
                end
            end
        end

        def interrupt
            return false
        end

        def execute opcode
            t = Time.now
            t_states = 4
            case opcode
            when 0x00 #NOP
            when 0x01 #LD BC,HHLL
                @bc.copy(@pc.read16(@memory))
                t_states = 10
                op_size = 3
            when 0x02 #LD (BC),A
                @memory[@bc.value].copy(@a)
                t_states = 7
            when 0x03 #INC BC
                @bc.store(@bc.value + 1)
                t_states = 6
            when 0x04 #INC B
                @b.store(@b.value + 1)
                @f.flag_n = false
                @b.flags(@f)
            when 0x05 #DEC B
                @b.store(@b.value - 1)
                @f.flag_n = true
                @b.flags(@f)
            when 0x06 #LD B,NN
                @b.copy(@pc.read8(@memory))
                t_states = 7
                op_size = 2
            when 0x07 #RLCA
                @a.rotate_left
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = @a.carry
            when 0x08 #EX AF,AF’
                @a.exchange(@a’)
                @f.exchange(@f’)
            when 0x09 #ADD HL,BC
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x0A #LD A,(BC)
                @a.copy(@memory[@bc.value])
                t_states = 7
            when 0x0B #DEC BC
                @bc.store(@bc.value - 1)
                t_states = 6
            when 0x0C #INC C
                @c.store(@c.value + 1)
                @f.flag_n = false
                @c.flags(@f)
            when 0x0D #DEC C
                @c.store(@c.value - 1)
                @f.flag_n = false
                @c.flags(@f)
            when 0x0E #LD C,NN
                @c.copy(@pc.read8(@memory))
                t_states = 7
            when 0x0F #RRCA
                @a.rotate_right
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = !@a.carry
            when 0x10 #DJNZ NN
                val = @pc.read8(@memory).value
                @b.store(@b.value - 1)
                if @b.nonzero?
                    @pc.store(@pc.value + val)
                    t_states = 13
                else
                    t_states = 8
                end
            when 0x11 #LD DE,HHLL
                @de.copy(@pc.read16(@memory))
                t_states = 10
            when 0x12 #LD (DE),A
                @memory[@de.value].copy(@a)
                t_states = 7
            when 0x13 #INC DE
                @de.store(@de.value + 1)
                t_states = 6
            when 0x14 #INC D
                @d.store(@d.value + 1)
                @f.flag_n = false
                @d.flags(@f)
            when 0x15 #DEC D
                @d.store(@d.value - 1)
                @f.flag_n = true
                @d.flags(@f)
            when 0x16 #LD D,NN
                @d.store(@pc.read8(@memor))
                t_states = 7
            when 0x17 #RLA
                @a.rotate_left_trough_carry @f.flag_c
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = !@a.carry
            when 0x18 #JR NN
                @pc.store(@pc.value + @pc.read8(@memory).value)
                t_states = 12
            when 0x19 #ADD HL,DE
                @hl.store(@hl.value + @bc.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x1A #LD A,(DE)
                @a.copy(@memory[@de.value])
                t_states = 7
            when 0x1B #DEC DE
                @de.store(@de.value - 1)
                t_states = 6
            when 0x1C #INC E
                @e.store(@e.value + 1)
                @f.flag_n = false
                @e.flags(@f)
            when 0x1D #DEC E
                @e.store(@e.value - 1)
                @f.flag_n = true
                @e.flags(@f)
            when 0x1E #LD E,NN
                @e.copy(@pc.read8(@memory))
                t_states = 7
            when 0x1F #RRA
                @a.rotate_right_trough_carry @f.flag_c
                @f.flag_n, @f.flag_hc = false
                @f.flag_c = !@a.carry
            when 0x20 #JR NZ,NN
                val = @pc.read8(@memory).value
                if @f.flag_z
                    t_states = 7
                else
                    @pc.store(@pc.value + val)
                    t_states = 12
                end
            when 0x21 #LD HL,HHLL
                @hl.copy(@pc.read16(@memory))
                t_states = 10
            when 0x22 #LD (HHLL),HL
                v = @pc.read16(@memory).value
                Register16.new(@memory[v + 1], @memory[v]).copy(@hl)
                t_states = 16
            when 0x23 #INC HL
                @hl.store(@hl.value + 1)
                t_states = 6
            when 0x24 #INC H
                @h.store(@h.value + 1)
                @f.flag_n = false
                @h.flags(@f)
            when 0x25 #DEC H
                @h.store(@h.value - 1)
                @f.flag_n = true
                @h.flags(@f)
            when 0x26 #LD H,NN
                @h.copy(@pc.read8(@memory))
                t_states = 7
            when 0x27 #DAA
                q, r = @a.as_unsigned.divmod MAX4
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
                @f.parity(@a.value)
                @f.flag_z = (@a.value == 1)
                @f.flag_s = @a.value.negative?
            when 0x28 #JR Z,NN
                val = @pc.read8(@memory).value
                if @f.flag_z
                    @pc.store(@pc.value + val)
                    t_states = 12
                else
                    t_states = 7
                end
            when 0x29 #ADD HL,HL
                @hl.store(@hl.value + @hl.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x2A #LD HL,(HHLL)
                v = @pc.read16(@memory).value
                @hl.copy(Register16.new(@memory[v + 1].value, @memory[v].value))
                t_states = 16
            when 0x2B #DEC HL
                @hl.store(@hl.value - 1)
                t_states = 6
            when 0x2C #INC L
                @l.store(@l.value + 1)
                @f.flag_n = false
                @l.flags(@f)
            when 0x2D #DEC L
                @l.store(@l.value - 1)
                @f.flag_n = true
                @l.flags(@f)
            when 0x2E #LD L,NN
                @l.copy(@pc.read8(@memory))
                t_states = 7
            when 0x2F #CPL
                @a.store(~(@a.as_unsigned + MAX16))
                @f.flag_n, @f.flag_hc = true
            when 0x30 #JR NC,NN
                val = @pc.read8(@memory).value
                if @f.flag_c
                    t_states = 7
                else
                    @pc.store(@pc.value + val)
                    t_states = 12
                end
            when 0x31 #LD SP,HHLL
                @sp.copy(@pc.read16(@memory))
                t_states = 10
            when 0x32 #LD (HHLL),A
                @memory[@pc.read16(@memory).value].copy(@a)
                t_states = 16
            when 0x33 #INC SP
                @sp.store(@sp.value + 1)
                t_states = 6
            when 0x34 #INC (HL)
                m = @memory[@hl.value]
                m.store(m.value + 1)
                @f.flag_n = true
                m.flags(@f)
                t_states = 11
            when 0x35 #DEC (HL)
                m = @memory[@hl.value]
                m.store(m.value - 1)
                @f.flag_n = true
                m.flags(@f)
                t_states = 11
            when 0x36 #LD (HL),NN
                @memory[@hl.value].copy(@pc.read8(@memory))
                t_states = 10
            when 0x37 #SCF
                @f.flag_c = true
                @f.flag_n, @f.flag_hc = false
            when 0x38 #JR C,NN
                val = @pc.read8(@memory).value
                if @f.flag_c
                    @pc.store(@pc.value + val)
                    t_states = 12
                else
                    t_states = 7
                end
            when 0x39 #ADD HL,SP
                @hl.store(@hl.value + @sp.value)
                @f.flag_n = false
                @hl.flags_math(@f)
                t_states = 11
            when 0x3A #LD A,(HHLL)
                @a.copy(@memory[@pc.read16(@memory).value])
                t_states = 13
            when 0x3B #DEC SP
                @sp.store(@sp.value - 1)
                t_states = 6
            when 0x3C #INC A
                @a.store(@a.value + 1)
                @f.flag_n = false
                @a.flags(@f)
            when  0x3D #DEC A
                @a.store(@a.value - 1)
                @f.flag_n = false
                @a.flags(@f)
            when 0x3E #LD A,NN
                @a.copy(@pc.read8(@memory))
                t_states = 7
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
                t_states = 7
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
                t_states = 7
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
                t_states = 7
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
                t_states = 7
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
                t_states = 7
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
                t_states = 7
            when 0x6F #LD L,A
                @l.copy(@a)
            when 0x70 #LD (HL),B
                @memory[@hl.value].copy(@b)
                t_states = 7
            when 0x71 #LD (HL),C
                @memory[@hl.value].copy(@c)
                t_states = 7
            when 0x72 #LD (HL),D
                @memory[@hl.value].copy(@d)
                t_states = 7
            when 0x73 #LD (HL),E
                @memory[@hl.value].copy(@e)
                t_states = 7
            when 0x74 #LD (HL),H
                @memory[@hl.value].copy(@h)
                t_states = 7
            when 0x75 #LD (HL),L
                @memory[@hl.value].copy(@l)
                t_states = 7
            when 0x76 #HALT
                @can_execute = false
            when 0x77 #LD (HL),A
                @memory[@hl.value].copy(@a)
                t_states = 7
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
                t_states = 7
            when 0x7F #LD A,A
            when 0x80 #ADD A,B
                @a.add(@b.value, @f)
            when 0x81 #ADD A,C
                @a.add(@c.value, @f)
            when 0x82 #ADD A,D
                @a.add(@d.value, @f)
            when 0x83 #ADD A,E
                @a.add(@e.value, @f)
            when 0x84 #ADD A,H
                @a.add(@h.value, @f)
            when 0x85 #ADD A,L
                @a.add(@l.value, @f)
            when 0x86 #ADD A,(HL)
                @a.add(@memory[@hl.value].value, @f)
                t_states = 7
            when 0x87 #ADD A,A
                @a.add(@a.value, @f)
            when 0x88 #ADC A,B
                @a.add(@b.value + (@f.carry ? 1 : 0), @f)
            when 0x89 #ADC A,C
                @a.add(@c.value + (@f.carry ? 1 : 0), @f)
            when 0x8A #ADC A,D
                @a.add(@d.value + (@f.carry ? 1 : 0), @f)
            when 0x8B #ADC A,E
                @a.add(@e.value + (@f.carry ? 1 : 0), @f)
            when 0x8C #ADC A,H
                @a.add(@h.value + (@f.carry ? 1 : 0), @f)
            when 0x8D #ADC A,L
                @a.add(@l.value + (@f.carry ? 1 : 0), @f)
            when 0x8E #ADC A,(HL)
                @a.add(@memory[@hl.value].value + (@f.carry ? 1 : 0), @f)
                t_states = 7
            when 0x8F #ADC A,A
                @a.add(@a.value + (@f.carry ? 1 : 0), @f)
            when 0x90 #SUB A,B
                @a.sub(@b.value, @f)
            when 0x91 #SUB A,C
                @a.sub(@c.value, @f)
            when 0x92 #SUB A,D
                @a.sub(@d.value, @f)
            when 0x93 #SUB A,E
                @a.sub(@e.value, @f)
            when 0x94 #SUB A,H
                @a.sub(@h.value, @f)
            when 0x95 #SUB A,L
                @a.sub(@l.value, @f)
            when 0x96 #SUB A,(HL)
                @a.sub(@memory[@hl.value].value, @f)
                t_states = 7
            when 0x97 #SUB A,A
                @a.sub(@a.value, @f)
            when 0x98 #SBC A,B
                @a.sub(@a.value + (@f.carry ? 1 : 0), @f)
            when 0x99 #SBC A,C
                @a.sub(@c.value + (@f.carry ? 1 : 0), @f)
            when 0x9A #SBC A,D
                @a.sub(@d.value + (@f.carry ? 1 : 0), @f)
            when 0x9B #SBC A,E
                @a.sub(@e.value + (@f.carry ? 1 : 0), @f)
            when 0x9C #SBC A,H
                @a.sub(@h.value + (@f.carry ? 1 : 0), @f)
            when 0x9D #SBC A,L
                @a.sub(@l.value + (@f.carry ? 1 : 0), @f)
            when 0x9E #SBC A,(HL)
                @a.sub(@memory[@hl.value].value + (@f.carry ? 1 : 0), @f)
                t_states = 7
            when 0x9F #SBC A,A
                @a.sub(@a.value + (@f.carry ? 1 : 0), @f)
            when 0xA0 #AND A,B
                @a.and(@b.value, @f)
            when 0xA1 #AND A,C
                @a.and(@c.value, @f)
            when 0xA2 #AND A,D
                @a.and(@d.value, @f)
            when 0xA3 #AND A,E
                @a.and(@e.value, @f)
            when 0xA4 #AND A,H
                @a.and(@h.value, @f)
            when 0xA5 #AND A,L
                @a.and(@l.value, @f)
            when 0xA6 #AND (HL)
                @a.and(@memory[@hl.value].value, @f)
                t_states = 7
            when 0xA7 #AND A,A
                @a.and(@a.value, @f)
            when 0xA8 #XOR A,B
                @a.xor(@b.value, @f)
            when 0xA9 #XOR A,C
                @a.xor(@c.value, @f)
            when 0xAA #XOR A,D
                @a.xor(@d.value, @f)
            when 0xAB #XOR A,E
                @a.xor(@e.value, @f)
            when 0xAC #XOR A,H
                @a.xor(@h.value, @f)
            when 0xAD #XOR A,L
                @a.xor(@l.value, @f)
            when 0xAE #XOR A,(HL)
                @a.xor(@memory[@hl.value].value, @f)
                t_states = 7
            when 0xAF #XOR A,A
                @a.xor(@a.value, @f)
            when 0xB0 #OR A,B
                @a.or(@b.value, @f)
            when 0xB1 #OR A,C
                @a.or(@c.value, @f)
            when 0xB2 #OR A,D
                @a.or(@d.value, @f)
            when 0xB3 #OR A,E
                @a.or(@e.value, @f)
            when 0xB4 #OR A,H
                @a.or(@h.value, @f)
            when 0xB5 #OR A,L
                @a.or(@l.value, @f)
            when 0xB6 #OR A,(HL)
                @a.or(@memory[@hl.value].value, @f)
                t_states = 7
            when 0xB7 #OR A,A
                @a.or(@a.value, @f)
            when 0xB8 #CP A,B
                @f.flag_z = (@a.value == @b.value)
            when 0xB9 #CP A,C
                @f.flag_z = (@a.value == @c.value)
            when 0xBA #CP A,D
                @f.flag_z = (@a.value == @d.value)
            when 0xBB #CP A,E
                @f.flag_z = (@a.value == @e.value)
            when 0xBC #CP A,H
                @f.flag_z = (@a.value == @h.value)
            when 0xBD #CP A,L
                @f.flag_z = (@a.value == @l.value)
            when 0xBE #CP A,(HL)
                @f.flag_z = (@a.value == @memory[@hl.value].value)
            when 0xBF #CP A,A
                @f.flag_z = true
            when 0xC0 #RET NZ
                if @f.flag_z
                    t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15
                end
            when 0xC1 #POP BC
                @bc.copy(@sp.read16(@memory))
                t_states = 10
            when 0xC2 #JP NZ,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_z
                t_states = 10
            when 0xC3 #JP HHLL
                @pc.copy(@pc.read16(@memory))
                t_states = 10
            when 0xC4 #CALL NZ,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_z
                    t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                end
            when 0xC5 #PUSH BC
                @sp.push(@memory).copy(@bc)
                t_states = 10
            when 0xC6 #ADD A,NN
                @a.add(@pc.read8(@memory).value, @f)
                t_states = 7
            when 0xC7 #RST 00
                @sp.push(@memory).copy(@pc)
                @pc.copy(0)
                t_states = 11
            when 0xC8 #RET Z
                if @f.flag_z
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15
                else
                    t_states = 11
                end
            when 0xC9 #RET
                @pc.copy(@sp.read16(@memory))
                t_states = 10
            when 0xCA #JP Z,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if @f.flag_z
                t_states = 10
            when 0xCB #CB
                #TODO: CB
                case @pc.read8(@memory)
                when 0x00 #RLC B
                    @b.rotate_left
                    @f.flag_c = @b.carry
                    @f.s_z(@b)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@b.value)
                when 0x01 #RLC C
                    @c.rotate_left
                    @f.flag_c = @c.carry
                    @f.s_z(@c)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@c.value)
                when 0x02 #RLC D
                    @d.rotate_left
                    @f.flag_c = @d.carry
                    @f.s_z(@d)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@d.value)
                when 0x03 #RLC E
                    @e.rotate_left
                    @f.flag_c = @e.carry
                    @f.s_z(@e)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@e.value)
                when 0x04 #RLC H
                    @h.rotate_left
                    @f.flag_c = @h.carry
                    @f.s_z(@h)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@h.value)
                when 0x05 #RLC L
                    @l.rotate_left
                    @f.flag_c = @l.carry
                    @f.s_z(@l)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@l.value)
                when 0x06 #RLC (HL)
                    reg = @memory[@hl.value]
                    reg.rotate_left
                    @f.flag_c = @reg.carry
                    @f.s_z(@reg)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@reg.value)
                when 0x07 #RLC A
                    @a.rotate_left
                    @f.flag_c = @a.carry
                    @f.s_z(@a)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@a.value)
                when 0x08 #RRC B
                    @b.rotate_right
                    @f.flag_c = @b.carry
                    @f.s_z(@b)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@b.value)
                when 0x09 #RRC C
                    @c.rotate_right
                    @f.flag_c = @c.carry
                    @f.s_z(@c)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@c.value)
                when 0x0A #RRC D
                    @d.rotate_right
                    @f.flag_c = @d.carry
                    @f.s_z(@d)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@d.value)
                when 0x0B #RRC E
                    @e.rotate_right
                    @f.flag_c = @e.carry
                    @f.s_z(@e)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@e.value)
                when 0x0C #RRC H
                    @h.rotate_right
                    @f.flag_c = @h.carry
                    @f.s_z(@h)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@h.value)
                when 0x0D #RRC L
                    @l.rotate_right
                    @f.flag_c = @l.carry
                    @f.s_z(@l)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@l.value)
                when 0x0E #RRC (HL)
                    reg = @memory[@hl.value]
                    reg.rotate_right
                    @f.flag_c = reg.carry
                    @f.s_z(reg)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(reg.value)
                when 0x0F #RRC A
                    @a.rotate_right
                    @f.flag_c = @a.carry
                    @f.s_z(@a)
                    @f.flag_n, @f.flag_hc = false
                    @f.parity(@a.value)
                else
                    fail
                end
                t_states = 8
            when 0xCC #CALL Z,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_z
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                else
                    t_states = 10
                end
            when 0xCD #CALL HHLL
                reg = @pc.read16(@memory)
                @sp.push(@memory).copy(@pc)
                @pc.copy(reg)
                t_states = 17
            when 0xCE #ADC A,NN
                @a.add(@pc.read8(@memory).value + (@f.carry ? 1 : 0), @f)
                t_states = 7
            when 0xCF #RST 08
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x08)
                t_states = 11
            when 0xD0 #RET NC
                if @f.flag_c
                    t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15
                end
            when 0xD1 #POP DE
                @de.copy(@sp.read16(@memory))
                t_states = 10
            when 0xD2 #JP NC,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_c
                t_states = 10
            when 0xD3 #OUT (NN),A
                #TODO: OUT
                fail
            when 0xD4 #CALL NC,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_c
                    t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                end
            when 0xD5 #PUSH DE
                @sp.push(@memory).copy(@de)
                t_states = 10
            when 0xD6 #SUB A,NN
                @a.sub(@pc.read8(@memory), @f)
                t_states = 7
            when 0xD7 #RST 10
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x10)
                t_states = 11
            when 0xD8 #RET C
                if @f.flag_c
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15                    
                else
                    t_states = 11
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
                t_states = 10
            when 0xDB #IN A,(NN)
                #TODO: IN
                fail
            when 0xDC #CALL C,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_c
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                else
                    t_states = 10
                end
            when 0xDD #DD
                #TODO: DD
                fail
            when 0xDE #SBC A,NN
                @a.sub(@pc.read8(@memory) + (@f.carry ? 1 : 0), @f)
                t_states = 7
            when 0xDF #RST 18
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x18)
                t_states = 11
            when 0xE0 #RET PO
                if @f.flag_pv
                    t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15
                end
            when 0xE1 #POP HL
                @hl.copy(@sp.read16(@memory))
                t_states = 10
            when 0xE2 #JP PO,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_pv
                t_states = 10
            when 0xE3 #EX (SP),HL
                @l.exchange(@memory[@sp.value])
                @h.exchange(@memory[@sp.value + 1])
                t_states = 19
            when 0xE4 #CALL PO,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_pv
                    t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                end
            when 0xE5 #PUSH HL
                @sp.push(@memory).copy(@hl)
                t_states = 10
            when 0xE6 #AND A,NN
                @a.and(@pc.read8(@memory), @f)
            when 0xE7 #RST 20
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x20)
                t_states = 11
            when 0xE8 #RET PE
                if @f.flag_pv
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15
                else
                    t_states = 11
                end
            when 0xE9 #JP (HL)
                @pc.copy(Register16.new(@memory[@hl.value + 1], @memory[@hl.value]))
            when 0xEA #JP PE,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if @f.flag_pv
                t_states = 10
            when 0xEB #EX DE,HL
                @d.exchange(@h)
                @e.exchange(@l)
            when 0xEC #CALL PE,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_pv
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                else
                    t_states = 10
                end
            when 0xED #ED
                #TODO: ED
                fail
            when 0xEE #XOR A,NN
                @a.xor(@pc.read8(@memory), @f)
                t_states = 7
            when 0xEF #RST 28
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x28)
                t_states = 11
            when 0xF0 #RET P
                if @f.flag_s
                    t_states = 11
                else
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15
                end
            when 0xF1 #POP AF
                @af.copy(@sp.read16(@memory))
                t_states = 10
            when 0xF2 #JP P,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if !@f.flag_s
                t_states = 10
            when 0xF3 #DI
                can_interrupt = false
            when 0xF4 #CALL P,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_s
                    t_states = 10
                else
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                end
            when 0xF5 #PUSH AF
                @sp.push(@memory).copy(@af)
                t_states = 10
            when 0xF6 #OR A,NN
                @a.or(@pc.read8(@memory), @f)
                t_states = 7
            when 0xF7 #RST 30
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x30)
                t_states = 11
            when 0xF8 #RET M
                if @f.flag_s
                    @pc.copy(@sp.read16(@memory))
                    t_states = 15
                else
                    t_states = 11
                end
            when 0xF9 #LD SP,HL
                @sp.copy(@hl)
            when 0xFA #JP M,HHLL
                reg = @pc.read16(@memory)
                @pc.copy(reg) if @f.flag_s
                t_states = 10
            when 0xFB #EI
                @can_interrupt = true
            when 0xFC #CALL M,HHLL
                reg = @pc.read16(@memory)
                if @f.flag_s
                    @sp.push(@memory).copy(@pc)
                    @pc.copy(reg)
                    t_states = 17
                else
                    t_states = 10
                end
            when 0xDD #FD
                #TODO: FD
                fail
            when 0xFE #CP A,NN
                @f.flag_z = (@a.value == @pc.read8(@memory).value)
            when 0xFF #RST 38
                @sp.push(@memory).copy(@pc)
                @pc.copy(0x38)
                t_states = 11
            else
                fail
            end
            sleep(t + t_states * @state_duration - Time.now) / 1000.0
        end
    end

end

#TODO: how to set carry (for example on ADD A,A) ??
z80 = Z80::Z80.new
#z80.run
