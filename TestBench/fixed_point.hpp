#ifndef FIXED_POINT_H__
#define FIXED_POINT_H__

#include <ostream>
#include <iomanip>
#include <iostream>

#include "template_utils.hpp"

enum OverflowMode
{
    MASK,
    CLAMP
};

extern OverflowMode OVERFLOW_MODE;

/// A fixed-point integer type
/** \tparam INT_BITS The number of bits before the radix point
 *  \tparam FRAC_BITS The number of bits after the radix point
 *  \warning INT_BITS and FRAC_BITS must be non-negative, and their sum cannot exceed 64
 *
 *  Fixed point numbers are signed, so FixedPoint<5,2>, for example, has a
 *  range of -16.00 to +15.75
 *
 *  The internal storage (FixedPoint::raw_) is rounded up to the next highest
 *  power of 2, so a 17 bit number occupies 32 physical bits. Consider that the
 *  multiplication of a 5 bit fixed point (which occupies 8 bits of space) with
 *  an 10 bit fixed point (which occupies 16 bits of space) results in a 15 bit
 *  fixed point which occupies 16 bits of space.
 */
template <int INT_BITS, int FRAC_BITS>
struct FixedPoint
{
public:

    /// The integer type used internally to store the value
    typedef typename GET_INT_WITH_LENGTH<INT_BITS + FRAC_BITS>::RESULT RawType;

    typedef typename GET_INT_WITH_LENGTH<INT_BITS*2 + FRAC_BITS*2>::RESULT MultType;

    /// This is a type big enough to hold the largest integer value
    typedef typename GET_INT_WITH_LENGTH<INT_BITS>::RESULT IntType;

    /// This is the type that can handle the largest rounded integer value
    /** Note that this is slightly different to IntType because FixedPoint<3,1>(3.5).round() == 4 */
    typedef typename GET_INT_WITH_LENGTH<INT_BITS + 1>::RESULT RoundType;


    typedef FixedPoint<INT_BITS, FRAC_BITS> ThisType;

    struct RawValue
    {
        RawValue(RawType aValue) : value(aValue) { }
        RawType value;
    };

    /// Create a fixed-point with equivalent integer value
    /** For example in 4.12 fixed-point, the number "2" is 0010.000000000000  */
    FixedPoint(int value){
        if(OVERFLOW_MODE == OverflowMode::MASK){
            bool is_neg = value < 0;
            if (is_neg){
                value = -value;
            }
            raw_ = value << FRAC_BITS;
            mask = (1LL << (FRAC_BITS+INT_BITS)) - 1;
            applyMask();
            if (is_neg){
                raw_ = -raw_;
            }
        }
        else{
            if(value > max_val){
                raw_ = max_val;
            }
            else if(value < min_val){
                raw_ = min_val;
            }
            else{
                raw_ = value << FRAC_BITS;
            }
        }
    }

    // Terrible negative number handling, will fix it
    FixedPoint(double value){
        if(OVERFLOW_MODE == OverflowMode::MASK){
            bool is_neg = value < 0;
            if (is_neg){
                value = -value;
            }
            raw_ = value * ((__int128_t)1 << FRAC_BITS);
            mask = ((__int128_t)1 << (FRAC_BITS+INT_BITS)) - 1;
            applyMask();
            if (is_neg){
                raw_ = -raw_;
            }
        }
        else{
            if(value > max_val_f){
                raw_ = max_val;
            }
            else if(value < min_val_f){
                raw_ = min_val;
            }
            else{
                raw_ = value * ((__int128_t)1 << FRAC_BITS);
            }
        }
    }

    // Terrible negative number handling, will fix it
    // MASK MAY BE BUGGY TODO: 
    FixedPoint(float value){
        if(OVERFLOW_MODE == OverflowMode::MASK){
            bool is_neg = value < 0;
            if (is_neg){
                value = -value;
            }
            raw_ = value * (1 << FRAC_BITS);
            mask = (1LL << (FRAC_BITS+INT_BITS)) - 1;
            applyMask();
            if (is_neg){
                raw_ = -raw_;
            }
        }
        else{
            if(value > max_val_f){
                raw_ = max_val;
            }
            else if(value < min_val_f){
                raw_ = min_val;
            }
            else{
                raw_ = value * (1 << FRAC_BITS);
            }
        }
    }

    /// Default constructor
    FixedPoint(){
        raw_ = 0;
        mask = ((__int128_t)1 << (FRAC_BITS+INT_BITS)) - 1;
       //applyMask();
    }

    FixedPoint(RawValue value): raw_(value.value) {
        mask = (((__int128_t)1) << (FRAC_BITS+INT_BITS)) - ((__int128_t)1);
        //applyMask();
    }

    void applyMask(){
        raw_ &= mask;
    }

    /// Create a fixed-point with a predefined raw_ value, with no manipulation
    static FixedPoint<INT_BITS, FRAC_BITS> createRaw(RawType aData) { return FixedPoint<INT_BITS, FRAC_BITS>(RawValue(aData)); }

    /// Returns a new fixed-point that reinterprets the binary raw_.
    /** \warning This should be used sparingly since returns a number whos
      * value is not the necessarily same.
      * \note To just move the radix point, rather use LeftShift or RightShift  */
    template <int INT_BITS_NEW, int FRAC_BITS_NEW>
    FixedPoint<INT_BITS_NEW, FRAC_BITS_NEW> reinterpret()
    {
        return FixedPoint<INT_BITS_NEW, FRAC_BITS_NEW>::createRaw(raw_);
    }

    /// Make the integral part larger
    template <int INT_BITS_NEW>
    FixedPoint<INT_BITS_NEW, FRAC_BITS> extend()
    {
        return FixedPoint<INT_BITS_NEW, FRAC_BITS>::createRaw(raw_);
    }

    /// Returns a new fixed-point in a new format which is similar in value to the original
    /** This may result in loss of raw_ if the number of bits for either the
      * integer or fractional part are less than the original. */
    template <int INT_BITS_NEW, int FRAC_BITS_NEW>
    FixedPoint<INT_BITS_NEW, FRAC_BITS_NEW> convert() const
    {
        typedef typename FixedPoint<INT_BITS_NEW, FRAC_BITS_NEW>::RawType TargetRawType;

        return FixedPoint<INT_BITS_NEW, FRAC_BITS_NEW>::createRaw(
            CONVERT_FIXED_POINT<
                RawType,
                TargetRawType,
                (FRAC_BITS_NEW - FRAC_BITS),
                (FRAC_BITS_NEW > FRAC_BITS)
                >:: exec(raw_));
    }

    // This multiplication method also converts the result to a new format
    // This is mathematically correct, but I don't want it to happen for Eigen compatibility
    /*
    /// Multiplication with another fixed-point
    template <int INT_BITS2, int FRAC_BITS2>
    FixedPoint<INT_BITS + INT_BITS2, FRAC_BITS + FRAC_BITS2>
        operator *(FixedPoint<INT_BITS2, FRAC_BITS2> value) const
    {
        return FixedPoint<INT_BITS + INT_BITS2, FRAC_BITS + FRAC_BITS2>::createRaw(raw_ * value.getRaw());
    }
    */
   /*
   FixedPoint<INT_BITS, FRAC_BITS>
   operator *(FixedPoint<INT_BITS, FRAC_BITS> value) const
    {
        int256_t raw_shifted = raw_;
        int256_t value_shifted = value.getRaw();
        //int256_t raw_shifted = raw_ << FRAC_BITS;
        //int256_t value_shifted = value.getRaw() << FRAC_BITS;
        std::cout << "raw_shifted: " << raw_shifted << std::endl;
        std::cout << "value_shifted: " << value_shifted << std::endl;
        int256_t temp = raw_shifted * value_shifted;
        FixedPoint<INT_BITS*2, FRAC_BITS*2> temp_fp = FixedPoint<INT_BITS*2, FRAC_BITS*2>::createRaw(temp.convert_to<MultType>());
        std::cout << "temp: " << temp_fp.getRaw() << std::endl;
        // bitshift temp to the right
        int256_t temp_raw = temp_fp.getRaw();
        auto int_part = temp_raw >> (FRAC_BITS*2);
        auto frac_part = temp_raw & ((1LL << (FRAC_BITS)) - 1);
        std::cout << "int part: " << int_part << std::endl;
        std::cout << "frac part: " << frac_part << std::endl;
        //frac_part >>= FRAC_BITS*2;
        int256_t result = (int_part << FRAC_BITS) + (frac_part);
        return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(result.convert_to<RawType>());
        // Now we need to shift the radix point back to the original position
        //return temp.template convert<INT_BITS, FRAC_BITS>();
    }
    */
    // https://vanhunteradams.com/FixedPoint/FixedPoint.html
    FixedPoint<INT_BITS, FRAC_BITS>
    operator *(FixedPoint<INT_BITS, FRAC_BITS> value) const
    {
        int256_t raw_shifted = raw_;
        int256_t value_shifted = value.getRaw();
        int256_t mult = raw_shifted * value_shifted;
        // mask the last INT_BITS bits
        int256_t tmp1 = mult << INT_BITS;
        int256_t tmp2 = tmp1 >> INT_BITS;
        mult = tmp2;
        // get rid of underflow
        mult >>= FRAC_BITS;
        if(OVERFLOW_MODE == OverflowMode::MASK){
            return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(mult.convert_to<RawType>());
        }
        else{
            if(mult > max_val){
                return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(max_val);
            }
            else if(mult < min_val){
                return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(min_val);
            }
            else{
                return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(mult.convert_to<RawType>());
            }
        }
        //return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(mult.convert_to<RawType>());
    }
    // Multiplication with an integer
    // The intermediate type is set to some arbitrary value, there may be a better way to do this
    FixedPoint<INT_BITS, FRAC_BITS> operator *(int value) const
    {
        auto temp = FixedPoint<INT_BITS*2, FRAC_BITS*2>::createRaw(raw_ * value);
        // Now we need to shift the radix point back to the original position
        return temp.template convert<INT_BITS, FRAC_BITS>();
    }

    // Multiplication with a double
    FixedPoint<INT_BITS, FRAC_BITS> operator *(double value) const
    {
        auto temp = FixedPoint<INT_BITS*2, FRAC_BITS*2>::createRaw(raw_ * value);
        return temp.template convert<INT_BITS, FRAC_BITS>();
    }

    // *= overload
    FixedPoint<INT_BITS, FRAC_BITS>& operator*=(FixedPoint<INT_BITS, FRAC_BITS> value)
    {
        raw_ *= value.template convert<INT_BITS, FRAC_BITS>().getRaw();
        if(OVERFLOW_MODE == OverflowMode::MASK){
            applyMask();
        }
        else{
            if(raw_ > max_val){
                raw_ = max_val;
            }
            else if(raw_ < min_val){
                raw_ = min_val;
            }
        }
        return *this;
    }

    FixedPoint<INT_BITS, FRAC_BITS>& operator*=(int value)
    {
        raw_ *= FixedPoint<INT_BITS, FRAC_BITS>(value).getRaw();
        if(OVERFLOW_MODE == OverflowMode::MASK){
            applyMask();
        }
        else{
            if(raw_ > max_val){
                raw_ = max_val;
            }
            else if(raw_ < min_val){
                raw_ = min_val;
            }
        }
        return *this;
    }

    FixedPoint<INT_BITS, FRAC_BITS>& operator*=(double value)
    {
        raw_ *= FixedPoint<INT_BITS, FRAC_BITS>(value).getRaw();
        if(OVERFLOW_MODE == OverflowMode::MASK){
            applyMask();
        }
        else{
            if(raw_ > max_val){
                raw_ = max_val;
            }
            else if(raw_ < min_val){
                raw_ = min_val;
            }
        }
        return *this;
    }


    /// Addition with an integer
    FixedPoint<INT_BITS, FRAC_BITS> operator +(IntType value) const
    {
        return *this + FixedPoint<INT_BITS, FRAC_BITS>(value);
    }

    /// Addition with another fixed point
    FixedPoint<INT_BITS, FRAC_BITS>
        operator+(FixedPoint<INT_BITS, FRAC_BITS> value) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(value.template convert<INT_BITS, FRAC_BITS>());

        if(OVERFLOW_MODE == OverflowMode::MASK){
            return ResultType::createRaw(op1.getRaw() + op2.getRaw());
        }
        else{
            double val1 = op1.getValueF();
            double val2 = op2.getValueF();
            double result = val1 + val2;
            if(result > max_val_f){
                return ResultType::createRaw(max_val);
            }
            else if(result < min_val_f){
                return ResultType::createRaw(min_val);
            }
            else{
                return ResultType::createRaw(op1.getRaw() + op2.getRaw());
            }
        }
    }

    FixedPoint<INT_BITS, FRAC_BITS> operator +(float value) const
    {
        return *this + FixedPoint<INT_BITS, FRAC_BITS>(value);
    }

    ThisType& operator+=(FixedPoint<INT_BITS, FRAC_BITS> value)
    {
        raw_ += value.template convert<INT_BITS, FRAC_BITS>().getRaw();
        if(OVERFLOW_MODE == OverflowMode::MASK){
            applyMask();
        }
        else{
            if(raw_ > max_val){
                raw_ = max_val;
            }
            else if(raw_ < min_val){
                raw_ = min_val;
            }
        }
        return *this;
    }

    ThisType& operator+=(double value)
    {
        raw_ += ThisType(value).getRaw();
        return *this;
    }

    ThisType& operator+=(float value)
    {
        raw_ += ThisType(value).getRaw();
        return *this;
    }

    ThisType& operator-=(FixedPoint<INT_BITS, FRAC_BITS> value)
    {
        raw_ -= value.template convert<INT_BITS, FRAC_BITS>().getRaw();
        if(OVERFLOW_MODE == OverflowMode::MASK){
            applyMask();
        }
        else{
            if(raw_ > max_val){
                raw_ = max_val;
            }
            else if(raw_ < min_val){
                raw_ = min_val;
            }
        }
        return *this;
    }

    /// Subtraction operator
    FixedPoint<INT_BITS, FRAC_BITS>
        operator-(FixedPoint<INT_BITS, FRAC_BITS> value) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(value.template convert<INT_BITS, FRAC_BITS>());

        if(OVERFLOW_MODE == OverflowMode::MASK){
            return ResultType::createRaw(op1.getRaw() - op2.getRaw());
        }
        else{
            double val1 = op1.getValueF();
            double val2 = op2.getValueF();
            double result = val1 - val2;
            if(result > max_val_f){
                return ResultType::createRaw(max_val);
            }
            else if(result < min_val_f){
                return ResultType::createRaw(min_val);
            }
            else{
                return ResultType::createRaw(op1.getRaw() - op2.getRaw());
            }
        }
    }

    /// Subtraction operator
    FixedPoint<INT_BITS, FRAC_BITS> operator-(double value) const
    {
        return *this - FixedPoint<INT_BITS, FRAC_BITS>(value);
    }

    // Unary minus
    FixedPoint<INT_BITS, FRAC_BITS> operator-() const
    {
        return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(-raw_);
    }


    bool operator < (FixedPoint<INT_BITS, FRAC_BITS> other) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(other.template convert<INT_BITS, FRAC_BITS>());

        // Then the operation is trivial
        return op1.getRaw() < op2.getRaw();
    }

    bool operator > (FixedPoint<INT_BITS, FRAC_BITS> other) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(other.template convert<INT_BITS, FRAC_BITS>());

        // Then the operation is trivial
        return op1.getRaw() > op2.getRaw();
    }

    bool operator >= (FixedPoint<INT_BITS, FRAC_BITS> other) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(other.template convert<INT_BITS, FRAC_BITS>());

        // Then the operation is trivial
        return op1.getRaw() >= op2.getRaw();
    }

    bool operator <= (FixedPoint<INT_BITS, FRAC_BITS> other) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(other.template convert<INT_BITS, FRAC_BITS>());

        // Then the operation is trivial
        return op1.getRaw() <= op2.getRaw();
    }

    bool operator ==(FixedPoint<INT_BITS, FRAC_BITS> other) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(other.template convert<INT_BITS, FRAC_BITS>());

        // Then the operation is trivial
        return op1.getRaw() == op2.getRaw();
    }

    bool operator ==(double other) const
    {
        return getValueF() == other;
    }

    bool operator != (FixedPoint<INT_BITS, FRAC_BITS> other) const
    {
        // This is the type of the result
        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;

        // Convert both operands to result type
        ResultType op1(this->convert<INT_BITS, FRAC_BITS>());
        ResultType op2(other.template convert<INT_BITS, FRAC_BITS>());

        // Then the operation is trivial
        return op1.getRaw() != op2.getRaw();
    }

    /// Divide operator
    FixedPoint<INT_BITS, FRAC_BITS>
        operator/(FixedPoint<INT_BITS, FRAC_BITS> divisor) const
    {
        // (A/2^B)/(C/2^D) = (A/C)/2^(B-D);
        // fpm library says the normal fixed-point division is:
        // x * 2**Frac_bits / y

        typedef FixedPoint<INT_BITS, FRAC_BITS> ResultType;
        typedef typename GET_INT_WITH_LENGTH<INT_BITS*2 + FRAC_BITS*2>::RESULT IntermediateType;

        IntermediateType int_frac((__int128_t)1 << FRAC_BITS);

        // Expand the dividend so we don't lose resolution
        IntermediateType intermediate(raw_ * int_frac);
        // Shift the dividend. FRAC_BITS2 cancels with the fractional bits in
        // divisor, and INT_BITS2 adds the required resolution.
        //intermediate <<= FRAC_BITS + INT_BITS;
        //std::cout << "divisor: " << divisor.getRaw() << std::endl;
        //divisor += 0.001; // just so it doesn't divide by zero
        intermediate /= divisor.getRaw();

        return ResultType::createRaw(intermediate);
    }

    bool isfinite() const
    {
        return true;
    }

    // Divide with double
    FixedPoint<INT_BITS, FRAC_BITS>
        operator/(double divisor) const
    {
        typedef typename GET_INT_WITH_LENGTH<INT_BITS*2 + FRAC_BITS*2>::RESULT IntermediateType;

        IntermediateType int_frac(1LL << FRAC_BITS);

        // Expand the dividend so we don't lose resolution
        IntermediateType intermediate(raw_ * int_frac);
        // Shift the dividend. FRAC_BITS2 cancels with the fractional bits in
        // divisor, and INT_BITS2 adds the required resolution.
        //intermediate <<= FRAC_BITS + INT_BITS;

        intermediate /= divisor;

        return FixedPoint<INT_BITS, FRAC_BITS>::createRaw(intermediate);
    }

    // /= overload
    FixedPoint<INT_BITS, FRAC_BITS>& operator/=(FixedPoint<INT_BITS, FRAC_BITS> divisor)
    {
        typedef typename GET_INT_WITH_LENGTH<INT_BITS*2 + FRAC_BITS*2>::RESULT IntermediateType;

        IntermediateType int_frac(1LL << FRAC_BITS);

        // Expand the dividend so we don't lose resolution
        IntermediateType intermediate(raw_ * int_frac);
        // Shift the dividend. FRAC_BITS2 cancels with the fractional bits in
        // divisor, and INT_BITS2 adds the required resolution.
        //intermediate <<= FRAC_BITS + INT_BITS;

        intermediate /= divisor;

        return *this;
    }

    // an overload to be used when this type is converted to a double
    operator double() const
    {
        return getValueF();
    }

    FixedPoint<INT_BITS, FRAC_BITS>& operator=(const FixedPoint<INT_BITS, FRAC_BITS> value)
    {
        raw_ = value.getRaw();
        return *this;
    }

    FixedPoint<INT_BITS, FRAC_BITS>& operator=(const IntType value)
    {
        raw_ = FixedPoint<INT_BITS, FRAC_BITS>(value).getRaw();
        return *this;
    }

    FixedPoint<INT_BITS, FRAC_BITS>& operator=(const float value)
    {
        raw_ = FixedPoint<INT_BITS, FRAC_BITS>(value).getRaw();
        return *this;
    }

    FixedPoint<INT_BITS, FRAC_BITS>& operator=(const double value)
    {
        raw_ = FixedPoint<INT_BITS, FRAC_BITS>(value).getRaw();
        return *this;
    }

    /// Shift left by literal amount (just moves radix right)
    template <int AMT>
    FixedPoint<INT_BITS + AMT, FRAC_BITS - AMT> leftShift() const
    {
        return FixedPoint<INT_BITS + AMT, FRAC_BITS - AMT>::createRaw(raw_);
    }

    /// Shift right by literal amount (just moves radix left)
    template <int AMT>
    FixedPoint<INT_BITS - AMT, FRAC_BITS + AMT> rightShift() const
    {
        /// \todo Check that this does the right thing arithmetically with negative numbers
        return FixedPoint<INT_BITS - AMT, FRAC_BITS + AMT>::createRaw(raw_);
    }

    /// Write to an output stream
    std::ostream& emit(std::ostream& os) const
    {
        return os << std::fixed << std::setprecision((FRAC_BITS * 3 + 9) / 10) << getValueF();
    }

    /// For debugging: return the number of bits before the radix
    int getIntegralLength() const { return INT_BITS; }
    /// For debugging: return the number of bits after the radix
    int getFractionalLength() const { return FRAC_BITS; }
    /// Get the value as a floating point
    double getValueF() const { 
        //std::cout << "double val is " << (raw_)/(double)(1LL << FRAC_BITS) << std::endl;
        return (raw_)/(double)(1LL << FRAC_BITS); }
    /// Get the value truncated to an integer
    IntType getValue() const { return (IntType)(raw_ >> FRAC_BITS); }
    /// Get the value rounded to an integer (only works if FRAC_BITS > 0)
    RoundType round() const { return (RoundType)((raw_ + ((1 << FRAC_BITS) - 1)) >> FRAC_BITS); }
    /// Returns the raw internal binary contents
    RawType getRaw() const { return raw_; }

    static const int BIT_LENGTH = INT_BITS + FRAC_BITS;
    static const int FRAC_BITS_LENGTH = FRAC_BITS;
    static const int EXP_BITS_LENGTH = FRAC_BITS;


private:

    RawType raw_;
    __int128_t mask;
    __int128_t max_val = (__int128_t)(((int256_t)1 << (FRAC_BITS+INT_BITS-1)) - 1);
    __int128_t min_val = -(__int128_t)(((__int128_t)1 << (FRAC_BITS+INT_BITS-1)));
    double max_val_f = (double)max_val/(1LL << FRAC_BITS);
    double min_val_f = (double)min_val/(1LL << FRAC_BITS);
};

// Make the fixed-point struct  ostream outputtable
template <int INT_BITS, int FRAC_BITS>
std::ostream& operator<< (std::ostream &stream, const FixedPoint<INT_BITS, FRAC_BITS> &fixedPoint)
{
    return fixedPoint.emit(stream);
}

#endif /* end of include guard: FIXED_POINT_H__ */
