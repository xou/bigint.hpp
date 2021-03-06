#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <iostream>
#include <iomanip>
#include <cassert>

/**
 * A toy big integer implementation.
 *
 * Negative numbers are not fully supported yet.
 *
 */
class BigInt {

  public:
  typedef uint64_t internal_type;
  static const uint8_t internal_bitlen = sizeof(internal_type)*8;
  // no bits set
  static const internal_type internal_0 = (internal_type)0;
  // all bits set
  static const internal_type internal_max = ~((internal_type)0);

  /**
   * it is possible to use a different datatype as well, such as deque, to address
   * performance problems when using shifts a lot. However, this slows down other
   * operations.
   */
  typedef std::vector<internal_type> data_collection_type;
  private:
  /**
   * Internal data member. This must always be an unsigned type, and needs to have at least 2
   * bit width ;)
   *
   * The data blocks, bit/byte significance increases with vector index ("little endian"-ish).
   * The most significant block must never be 0.
   */
  data_collection_type m_data;
  /**
   * negative flag. Support is not fully here yet.
   */
  bool neg = false;

  public:
  /**
   * default copy constructor.
   */
  BigInt(const BigInt &) = default;
  BigInt& operator=(const BigInt & other) = default;

  /**
   * Initializes a BigInt with zero.
   */
  BigInt() : neg(false) {}

  /**
   * Initialize from another BigInt instance, but override the "negative" flag
   * with the value given in the second parameter.
   */
  BigInt(const BigInt &other, bool neg_override) {
    m_data = other.m_data;
    neg = neg_override;
  }

  /**
   * Initialize from a standard integer type. Note that the value
   * will be treated as unsigned.
   */
  BigInt(const uint64_t i, bool negative=false) {
    if (i == 0)
      return;
    m_data.push_back(i);
    neg = negative;
  }


  /**
   * Initialize from a string. The string is not checked for correctness. String parsing
   * is done RTL and stops at the first '-' or at the beginning of the string.
   *
   * Radix can be anything between 2-36. Supported characters are [0-9a-zA-Z].
   */
  BigInt(const std::string input, uint8_t radix=10) {
    // BitMath objects are used to calculate the positions where the bits for each digit are
    // placed. current_multiplier is multiplied by radix in each iteration with the radix.
    neg = false;
    BigInt radix_b(radix);
    BigInt current_multiplier(1ULL);

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
        if (current_char >= 'A' && current_char <= 'Z') {
          cval_i = 10+(current_char-'A');
        } else {
          cval_i = 10+(current_char-'a');
        }
      }
      BigInt cval(cval_i);
      cval *= current_multiplier;
      add_abs(cval);

      current_multiplier *= radix_b;
    }
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
    pos = pos * internal_bitlen + 1;

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
    internal_type mask = internal_max; // for s == 64, we'll copy all.
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
    if (s >= internal_bitlen) {
      // instantiate a new internal_type(internal_0) because insert wants a const reference
      // TODO this is one of the main performance bottlenecks for div_abs atm
      m_data.insert(m_data.begin(), s/internal_bitlen, internal_type(internal_0));
      s %= internal_bitlen;
      if (s == 0)
        return;
    }

    // if the highest set bit plus the shift points to the next element, we need an
    // extra element at the end:
    if (get_highest_set_bit_position() + s > internal_bitlen) {
      m_data.push_back(internal_type(internal_0));
    }


    for (int64_t i = m_data.size()-1; i > 0; --i) {
      m_data[i] <<= s;
      m_data[i] |= (m_data[i-1] >> (internal_bitlen - s));
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
    // iterate backwards so that this method can be used to add the object
    // to itself.
    for (ssize_t idx = other.m_data.size()-1; idx >= 0; --idx) {
      add_bits_at_pos(idx*internal_bitlen, other.m_data[idx]);
    }
  }

  /**
   * remove empty m_data blocks at the end.
   * this should be called by every function that could cause empty registers.
   */
  void remove_empty_registers() {
    ssize_t i = m_data.size()-1;
    for (; i >= 0; --i) {
      if (m_data[i])
        break;
    }
    m_data.erase(m_data.begin()+i+1, m_data.end());
    if (!m_data.size())
      neg = false;
    else
      assert(m_data[m_data.size()-1] != 0);
  }

  /**
   * subtract the absolute value of other from the absolute value of this object.
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
    // subtract the other way round and set the negative flag on the result.
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
   * n must neither be 0 nor larger than internal_bitlen.
   */
  internal_type get_bits_at_pos(uint64_t position, uint8_t n) const {
    uint64_t start_field_idx = position / internal_bitlen;
    if (start_field_idx >= m_data.size())
      return 0;

    // how far is the source mask shifted to the left?
    uint64_t start_field_shift = position % internal_bitlen;
    uint64_t end_field_idx = (position+n-1) / internal_bitlen;

    // calculate data from the first block, create a mask of n bits and shift it to the correct position.
    internal_type mask = internal_max;
    if (n % 64) {
      mask = ((1ULL << n)-1) << start_field_shift;
    } 
    // then shift the masked data from the source to the lower area of the result
    internal_type start_data = (m_data[start_field_idx] & mask) >> start_field_shift;
 
    if (start_field_idx == end_field_idx || end_field_idx >= m_data.size()) {
      return start_data;
    }

    // for the remaining bits, we need to know how large the overflow was, and extract those bytes.
    uint64_t remaining_bits = (start_field_shift + n) % internal_bitlen;
    mask = ((1ULL << remaining_bits)-1);
    // shift the resulting block to the MSB area (TODO we don't even need to mask, right?):
    internal_type total = (m_data[end_field_idx] & mask) << (internal_bitlen - remaining_bits);
    total |= start_data;
    return total;
  }

  /**
   * add the data from value at the position 'position'.
   * position is zero-indexed.
   */
  void add_bits_at_pos(const uint64_t& position, const internal_type& value) {
    if (value == 0)
      return;
    internal_type carry = 0;

    // if value is not a multiple of the internal_bitlen, split the
    // value into two words aligned to the bitlen, add the remaining bits (max: bitlen-1)
    // to the carry.
    uint64_t start_shift = position % internal_bitlen;
    uint64_t data_idx = position / internal_bitlen;

    // There are two "add" variables for the loop below: One to add
    // to the current block, one to add to the next one.
    // "next" carries the shifted value in the first iteration and the
    // carry in the next ones.
    internal_type current = value;
    internal_type next = carry;
    if (start_shift) {
      next = (value >> (internal_bitlen - start_shift));
      current = (value << start_shift);
    }

    // the maximum value of next here is 0x7f..., so it will always "survive" a += 1.
    do {
      if (data_idx >= m_data.size()) {
        m_data.resize(data_idx+1);
      }

      if (current) {
        // overflow check
        if (m_data[data_idx] > (internal_max - current)) {
          next += 1;
        }
        m_data[data_idx] += current;
      }

      current = next;
      next = 0;
      data_idx++;
    } while (current);
  }

  BigInt operator * (const BigInt &other) const {
    // multiplication of two binary digits will require at most 2*bit_len bits in the result.
    // maybe there is a assembly function where one can explicitly reference the overflowing
    // bits, but for now, take only half the input field, multiply it and add the resulting 2*bit_len
    // to the sum of the source positions.

    BigInt target(0);
    // this code should even support 7-bit architectures ;)
    const uint8_t blocksz = internal_bitlen/2;

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

  void operator *= (const BigInt &other) {
    BigInt tmp(*this * other);
    m_data = tmp.m_data;
    neg = tmp.neg;
  }

  /**
   * divide the current object by the denominator parameter and
   * return the result.
   *
   * TODO: This code depends quite a lot on shifts, which are quite
   *       slow unfortunately (mainly the memory allocator calls).
   */
  BigInt div_abs(const BigInt &denominator, BigInt &modulo) const {
    if (denominator.lt_abs(2)) {
      return BigInt(*this);
    }

    // Result
    BigInt quotient(0);
    // work variable from which fitting denominator_cp values will be subtracted in each
    // iteration (-> modulo)
    BigInt numerator(*this);

    uint64_t denominator_msb = denominator.get_highest_set_bit_position();


    while (denominator.le_abs(numerator)) {
      uint64_t numerator_msb = numerator.get_highest_set_bit_position();
      uint64_t shiftfactor = numerator_msb - denominator_msb;

      // work variable which can be shifted around. First, it is MSB-aligned with the numerator,
      // then it is checked whether it "fits". If not (that is, the less significant bits make the
      // numerator "higher" than the (shifted) denominator), shift one to the right and subtract -
      // this will always fit.
      BigInt denominator_cp(denominator);
      denominator_cp <<= shiftfactor;
      if (denominator_cp.le_abs(numerator)) {
        numerator.sub_abs(denominator_cp);
        quotient.add_bits_at_pos(shiftfactor, 1);
      } else {
        denominator_cp >>= 1;
        numerator.sub_abs(denominator_cp);
        quotient.add_bits_at_pos(shiftfactor-1, 1);
      }
    }
    modulo = numerator;
    return quotient;
  }

  BigInt div_abs(const BigInt& denominator) const {
    BigInt unused_modulo_result(0);
    return div_abs(denominator, unused_modulo_result);
  }

  /**
   * Division, obviously. Dividing by zero is equivalent to dividing by 1
   */
  BigInt operator / (const BigInt &denominator) const {
    return BigInt(div_abs(denominator), neg ^ denominator.neg);
  }

  /**
   * Modulo operation. The sign is taken from the dividend (same as the C++ ISO-2011 standard)
   */
  BigInt operator % (const BigInt &denominator) const {
    BigInt modulo_result(0);
    div_abs(denominator, modulo_result);
    modulo_result.neg = neg;
    return modulo_result;
  }

  /**
   * returns the string representation.
   * @param radix base to use
   * @param uppercase whether to use uppercase letters (for radix > 10)
   */
  std::string toString(uint8_t radix=10, bool uppercase=false) const {
    std::string ret;
    BigInt copy(*this);

    bool result_neg = copy.is_neg();

    while (!copy.is_zero()) {
      BigInt mod;
      copy = copy.div_abs(radix, mod);
      // figure out which character to use.
      if (mod.m_data.size() == 1) {
        // figure out which character to append.
        if (mod.m_data[0] < 10) {
          ret.push_back('0'+mod.m_data[0]);
        } else {
          if (uppercase) {
            ret.push_back('A' + mod.m_data[0]-10);
          } else {
            ret.push_back('a' + mod.m_data[0]-10);
          }
        }
      } else {
        ret.push_back('0');
      }
    }

    if (result_neg) {
      ret.push_back('-');
    }
    std::string reversed(ret.rbegin(), ret.rend());
    return reversed;
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

  /**
   * returns the internal representation of the data, without the neg flag.
   */
  data_collection_type get_internal_representation() {
    return m_data;
  }

  bool is_neg() {
    return neg;
  }

  bool is_zero() {
    return m_data.size() == 0;
  }

};
