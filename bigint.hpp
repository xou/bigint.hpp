#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <iomanip>

/**
 * Class that implements addition and multiplication in a horribly slow way, but it supports
 * numbers up to 2**(2**64) ;)
 *
 * (This is an artifact of the first steps in development, when i didn't yet have multiplication
 *  and addition in BigInt itself. Removal of this class is TODO )
 */
struct BitMath {
  std::set<uint64_t> bitPositions;

  BitMath() { }
  BitMath(uint64_t fromInt) {
    for (size_t p = 0; p < sizeof(uint64_t); ++p) {
      if (fromInt & (1ULL << p)) {
        bitPositions.insert(p);
      }
    }
  }
  BitMath(const BitMath &other) {
    bitPositions = other.bitPositions;
  }

  void operator += (const struct BitMath &other) {
    for (auto it = other.bitPositions.begin(); it != other.bitPositions.end(); ++it) {
      uint64_t pos = *it;
      while (bitPositions.count(pos)) {
        bitPositions.erase(pos++);
      }
      bitPositions.insert(pos);
    }
  };

  void operator *= (const struct BitMath &other) {
    std::set<uint64_t> newBitPositions;
    for (auto it = other.bitPositions.begin(); it != other.bitPositions.end(); ++it) {
      for (auto jt = bitPositions.begin(); jt != bitPositions.end(); ++jt) {
        uint64_t pos = *it + *jt;
        while (newBitPositions.count(pos)) {
          newBitPositions.erase(pos++);
        }
        newBitPositions.insert(pos);
      } 
    }
    bitPositions = newBitPositions;
  }
};

/**
 * A toy big integer implementation.
 *
 * Negative numbers are not fully supported yet.
 */
class BigInt {
  
  public:
  typedef uint64_t internal_type;
  const uint8_t internal_type_bitlen = sizeof(internal_type)*8;
  
  private:
  /**
   * The data blocks, bit/byte significance increases with vector index ("little endian"-ish).
   * The most significant block must never be 0.
   */
  std::vector<uint64_t> m_data;
  bool neg = false;

  public:
  BigInt(const uint64_t i, bool negative=false) {
    if (i == 0)
      return;
    m_data.push_back(i);
    neg = negative;
  }


  BigInt(const std::string input, uint8_t radix=10) { 
    // BitMath objects are used to calculate the positions where the bits for each digit are
    // placed. current_multiplier is multiplied by radix in each iteration with the radix.
    neg = false;
    BitMath radix_b(radix);
    BitMath current_multiplier(1ULL);

    for (ssize_t r_pos = input.size() - 1; r_pos >= 0; --r_pos) {
      uint8_t cval_i = 0;
      char current_char = input[r_pos];
      if (current_char == '-') {
        neg = true;
        break;
      }
      // calculate hex offsets
      if (radix <= 10 || (current_char >= '0' && current_char <= '9')) {
        cval_i = current_char - '0';
      } else {
        if (current_char >= 'A' && current_char <= 'F') {
          cval_i = 10+(current_char-'A');
        } else {
          cval_i = 10+(current_char-'a');
        }
      }
      BitMath cval(cval_i);
      cval *= current_multiplier;

      // for each digit, calculate radix*digit_value and add it to the data vector.
      // TODO make this code use BigInt instead of BitMath.
      for (auto it = cval.bitPositions.begin(); it != cval.bitPositions.end(); ++it) {
        const uint64_t bits_per_dtype = sizeof(internal_type)*8;
        uint64_t datavec_index = (*it)/bits_per_dtype;
        uint64_t invec_index = (*it)%bits_per_dtype;

        while (true) {
          if (datavec_index >= m_data.size()) {
            m_data.resize(datavec_index+1, 0);
          }
          if (!(m_data[datavec_index] & (1ULL << invec_index))) {
            m_data[datavec_index] |= (1ULL << invec_index);
            break;
          }
          m_data[datavec_index] ^= (1ULL << invec_index);
          // bubble up!
          invec_index++;
          if (invec_index/bits_per_dtype == 1) {
            invec_index = 0;
            datavec_index++;
          } 
        }
      }
      current_multiplier *= radix_b;
    }
  }

  std::vector<uint64_t> dump() {
    return m_data;
  }

  /**
   * returns the position of the highest bit set, or zero if no bit is set.
   * position index is 1-indexed.
   */
  uint64_t get_highest_set_bit_position() const {
    if (!m_data.size())
      return 0;
    
    uint64_t pos = m_data.size()-1;
    internal_type highest = m_data[pos]; 
    pos = pos * internal_type_bitlen + 1;

    // there is a nice way of doing binary search here:
    // if (highest & 0xffff0000) pos += 32;
    // if (highest & 0x00ff00ff) pos += 16; etc.
    // but it depends on the intenal_type_size, and needs to be generalized with templates or something.
    
    while (highest >>= 1)
      pos++;
    return pos;
  }

  void operator>>=(uint64_t s) {
    if (!m_data.size() || s == 0)
      return;

    // remove blocks that are shifted out completely.
    if (s > sizeof(internal_type)*8) {
      uint64_t delcount = s/(sizeof(internal_type)*8);
      if (delcount >= m_data.size()) {
        m_data.clear();
        return;
      }
      m_data.erase(m_data.begin(), m_data.begin()+delcount);
      s %= (sizeof(internal_type)*8);
    }

    // m_data now has at least one element and s is <= 64
    size_t i = 0;
    size_t mask = ~0ULL; // for s == 64, we'll copy all.
    if (s < sizeof(internal_type)*8) {
      mask = (1ULL << s) - 1; // create bitfield with s times 1 at the end.
    }

    for (; i < m_data.size()-1; ++i) {
      // TODO verify that this is in fact a logical shift, not an arithmetic shift (which would keep the sign bit)
      //      tests should cover that.
      //      -- verified that shr and shl instructions are generated. Might not be true when a
      //         signed internal type is used 
      
      // need to handle case s == num_bits seperately, since shr/shl by the wordlength has no effect (shift is effectively mod wordlength)
      if (s == sizeof(internal_type)*8) {
        m_data[i] = m_data[i+1];
      } else {
        // first, "make room" in the target block
        m_data[i] >>= s;
        // copy the lower s bits from the source block to the upper s bits of the target block
        m_data[i] |= (mask & m_data[i+1]) << (sizeof(internal_type)*8 - s);
      }
    }

    // finally, shift the last remaining element and remove if necessary
    m_data[i] >>= s;
    if (m_data[i] == 0 || s == sizeof(internal_type)*8) {
      m_data.resize(m_data.size()-1);
    }
  }


  void operator <<=(uint64_t s) {
    if (!m_data.size() || s == 0)
      return;

    // add empty elements to the front of the vector if s is larger than the internal bitlength
    if (s >= internal_type_bitlen) {
      m_data.insert(m_data.begin(), s/internal_type_bitlen, 0ULL);
      s %= internal_type_bitlen;
      if (s == 0)
        return;
    }

    // if the highest set bit plus the shift points to the next element, we need an
    // extra element at the end:
    if (get_highest_set_bit_position() + s > internal_type_bitlen) {
      m_data.push_back(0ULL);
    }


    for (int64_t i = m_data.size()-1; i > 0; --i) {
      m_data[i] <<= s;
      m_data[i] |= (m_data[i-1] >> (internal_type_bitlen - s));
    }
    m_data[0] <<= s;

  }
  
  /**
   * Returns |this| < |r|
   * ("less then" on the absolute values)
   */
  bool lt_abs(const BigInt &r) const {
    if (m_data.size() > r.m_data.size())
      return false;
    if (m_data.size() < r.m_data.size())
      return true;

    for (int i = m_data.size()-1; i >= 0; --i) {
      if (m_data[i] < r.m_data[i])
        return true;
      if (m_data[i] > r.m_data[i])
        return false;
    }
    // equal.
    return false;
  }

  /**
   * returns |this| == |r|
   */
  bool eq_abs(const BigInt &r) const {
    if (m_data.size() != r.m_data.size())
      return false;
    for (size_t i = 0; i < m_data.size(); ++i) {
      if (m_data[i] != r.m_data[i])
        return false;
    }
    return true;
  }

  bool operator< (const BigInt &r) const {
    if (m_data.size() == 0 && r.m_data.size())
      return false;

    if (neg && !r.neg) // we know |this| > 0 and |r| > 0
      return true;
    if (!neg && r.neg)
      return false;


    if (m_data.size() > r.m_data.size())
      return false;
    if (m_data.size() < r.m_data.size())
      return true;
    
 
    for (int i = m_data.size()-1; i >= 0; --i) {
      if (neg ^ (m_data[i] < r.m_data[i]))
        return true;
      if (neg ^ (m_data[i] > r.m_data[i]))
        return false;
    }
    return false;
    // walk from the highest index down, compare each word.
  }


  bool operator==(const BigInt &r) const {
    if (neg != r.neg) {
      return false;
    }
    return eq_abs(r);
  }

  bool operator <= (const BigInt &r) const {
    // TODO efficiency...
    return (*this == r) || (*this < r);
  }

  bool le_abs(const BigInt &r) const {
    return lt_abs(r) || eq_abs(r);
  }

  bool operator != (const BigInt &r) const {
    return !(*this == r);
  }

  /**
   * Add the absolute value of other to this object's absolute value.
   */
  void add_abs( const BigInt &other) {
    // TODO this *should* work with the object itself, but this must be verified!
    uint16_t carry_in = 0;
    uint16_t carry_out = 0;
    uint64_t idx = 0;
    for (; idx < other.m_data.size() || idx < m_data.size() || carry_out; ++idx) {
      carry_in = carry_out;
      internal_type other_data = 0ULL;
      if (idx < other.m_data.size()) {
        other_data = other.m_data[idx];
      }

      uint64_t local_data = 0ULL;
      if (m_data.size() <= idx) {
        m_data.resize(m_data.size()+1);
      } else {
        local_data = m_data[idx];
      }

      // set carry out if addition of both values will cause overflow (1.)
      carry_out = (other_data > (~0ULL - local_data));

      local_data += other_data;
      
      // also, set carry if the carry in causes overflow (2.; both cases cannot occur at the same time.)
      if (local_data == ~0ULL && carry_in) {
        carry_out = 1;
      }
      m_data[idx] = carry_in + local_data;
    }
  }

  void remove_empty_registers() {
    uint64_t i = m_data.size()-1;
    for (; i >= 0; --i) {
      if (m_data[i])
        break;
    }
    m_data.erase(m_data.begin()+i+1, m_data.end());
    if (!m_data.size())
      neg = false;
  }

  /**
   * substract the absolute value of other from the absolute value of this object.
   * |other| must be lower than or equal to |this|.
   */
  void sub_abs( const BigInt &other) {
    if (other.m_data.size() > m_data.size()) {
      m_data.clear();
      return;
    }

    uint8_t carry_in = 0, carry_out = 0;
    for (uint64_t i = 0; i < m_data.size(); ++i) {
      carry_out = 0;
      if (other.m_data.size() > i) {
        if (other.m_data[i] > m_data[i]) {
          carry_out = 1;
        }
        m_data[i] -= other.m_data[i];
      }
      if (carry_in > m_data[i]) {
        carry_out = 1;
      }
      m_data[i] -= carry_in;
      carry_in = carry_out;
    }
    remove_empty_registers();
  }

  void operator += (const BigInt &other) {
    if (neg == other.neg) {
      add_abs(other);
    } else {
      if (le_abs(other)) {
        sub_abs(other);
        // we are larger than other, so our negative flag stays (sub_abs will remove it if equal).
      } else {
        BigInt tmp = other;
        tmp.sub_abs(*this);
        m_data = tmp.m_data;
        neg = other.neg;
      }
    }
  }

  void operator -= (const BigInt &other) {
    // Make sure that the other number is smaller than this. If not,
    // substract the other way round and set the negative flag on the result.
    if (lt_abs(other)) {
      BigInt tmp(other);
      tmp -= *this;
      m_data = tmp.m_data;
      neg = true;
      return;
    }
    // "other"'s absolute value is now lower than ours.
    if (neg == other.neg) {
      sub_abs(other);
    } else {
      add_abs(other);
    }
  }

  /**
   * return n bits starting from the given position (0-based from LSB).
   * all bits higher than MSB will be zero.
   *
   * n must neither be 0 nor larger than internal_type_bitlen.
   */
  internal_type get_bits_at_pos(uint64_t position, uint8_t n) const {
    uint64_t start_field_idx = position / internal_type_bitlen;
    if (start_field_idx >= m_data.size())
      return 0;

    // how far is the source mask shifted to the left?
    uint64_t start_field_shift = position % internal_type_bitlen;
    uint64_t end_field_idx = (position+n-1) / internal_type_bitlen;

    // calculate data from the first block, create a mask of n bits and shift it to the correct position.
    internal_type mask = ~0ULL;
    if (n % 64) {
      mask = ((1ULL << n)-1) << start_field_shift;
    } 
    // then shift the masked data from the source to the lower area of the result
    internal_type start_data = (m_data[start_field_idx] & mask) >> start_field_shift;
 
    if (start_field_idx == end_field_idx || end_field_idx >= m_data.size()) {
      return start_data;
    }

    // for the remaining bits, we need to know how large the overflow was, and extract those bytes.
    uint64_t remaining_bits = (start_field_shift + n) % internal_type_bitlen;
    mask = ((1ULL << remaining_bits)-1);
    // shift the resulting block to the MSB area (TODO we don't even need to mask, right?):
    internal_type total = (m_data[end_field_idx] & mask) << (internal_type_bitlen - remaining_bits);
    total |= start_data;
    return total;
  }

  /**
   * add the data from value at the position 'position'.
   */
  void add_bits_at_pos(uint64_t position, internal_type value) {
    if (value == 0)
      return;
    internal_type carry = 0;

    // if value is not a multiple of the internal_type_bitlen, split the
    // value into two words aligned to the bitlen, add the remaining bits (max: bitlen-1)
    // to the carry.
    
    uint64_t start_shift = position % internal_type_bitlen; 
    uint64_t data_idx = position / internal_type_bitlen;

    if (start_shift) {
      carry = (value >> (internal_type_bitlen - start_shift));
      value <<= start_shift;  
    }


    do {
      uint64_t carry_out = 0;
      if (data_idx >= m_data.size()) {
        m_data.resize(data_idx+1);
      }

      // first iteration only:
      if (value) {
        if (m_data[data_idx] > (~0ULL-value)) {
          // we can safely add 1 to carry here, since the maximum value of
          // carry (through the alignment-shift) is 0x7f... (MSB is definitely 0)
          carry += 1;
        } 
        m_data[data_idx] += value;
        value = 0;
      } else {
        // following iterations, value is zero and carry is something up to 0x80
        if (m_data[data_idx] > (~0ULL-carry)) { 
          carry_out = 1;
        } else {
          carry_out = 0;
        }
        m_data[data_idx] += carry;
        carry = carry_out;
      }

      data_idx++; 
    } while (carry);
  }

  BigInt operator * (const BigInt &other) const {
    // multiplication of two binary digits will require at most 2*bit_len bits in the result.
    // maybe there is a assembly function where one can explicitly reference the overflowing
    // bits, but for now, take only half the input field, multiply it and add the resulting 2*bit_len
    // to the sum of the source positions. 
    
    BigInt target(0);
    // this code should even support 7-bit architectures ;)
    const uint8_t blocksz = internal_type_bitlen/2;

    // iterate over all blocks of blocksz in other
    const uint64_t other_msb = other.get_highest_set_bit_position();
    const uint64_t this_msb = get_highest_set_bit_position();

    // TODO is it better to iterate the other way round? Also, verify that there is not an off-by-one error here (b/c get_highest_set_bit_pos is 1-indexed)
    for (uint64_t other_position = 0; other_position <= other_msb; other_position += blocksz) {
      const internal_type other_block = other.get_bits_at_pos(other_position, blocksz);
      // iterate over all blocks of blocksz in the internal buffer. We need to refresh the position
      // each iteration since it will likely grow.
      for (uint64_t this_position = 0; this_position <= this_msb; this_position += blocksz) {
        const internal_type this_block = get_bits_at_pos(this_position, blocksz);
        target.add_bits_at_pos(other_position + this_position, this_block * other_block);
      }
    }

    return target;
  }

  /**
   * Division, obviously. Dividing by zero is equivalent to dividing by 1
   */
  BigInt operator / (const BigInt &denominator) const {
    if (denominator.lt_abs(2)) {
      return BigInt(*this);
    }
    // Result
    BigInt quotient(0);
    // work variable from which fitting denominator_cp values will be substracted in each
    // iteration.
    BigInt numerator(*this);

    uint64_t denominator_msb = denominator.get_highest_set_bit_position();
    while (denominator.le_abs(numerator)) {
      uint64_t numerator_msb = numerator.get_highest_set_bit_position();
      uint64_t shiftfactor = numerator_msb - denominator_msb;

      BigInt factor(1);

      // Work variable which will be shifted around to see whether it "fits" into numerator.
      BigInt denominator_cp(denominator);

      // Align the two MSBs. We can either substract directly with this result
      // or with the denominator shifted to the right by one.
      factor <<= shiftfactor;
      denominator_cp <<= shiftfactor;
      if (denominator_cp.le_abs(numerator)) {
        numerator.sub_abs(denominator_cp);
        quotient += factor;
      } else {
        factor >>= 1;
        denominator_cp >>= 1;
        numerator.sub_abs(denominator_cp);
        quotient += factor;
      }
    }
    return quotient;
  }

  void dump_registers(std::string prefix = "", int fill = 4) const {
    std::cout << prefix << " " << m_data.size() << " " << (neg?" (-)":" (+)");

    for (;fill > (ssize_t)m_data.size(); --fill) {
      std::cout << "  0x" << std::setw(sizeof(internal_type)*2) << std::setfill('0') << std::hex << 0x0ULL << std::dec;
    }
    for (ssize_t i = m_data.size()-1; i >= 0; --i) {
      std::cout <<  "  0x" << std::setw(sizeof(internal_type)*2) << std::setfill('0') << std::hex << m_data[i] << std::dec;
    }
    std::cout << std::endl;
  }

};
