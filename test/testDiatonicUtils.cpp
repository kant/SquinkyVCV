#include "DiatonicUtils.h"
#include "asserts.h"

// simple reality check
static void assertTransposeValidC(const std::vector<int> _xpose, int amount)
{
    assertEQ(_xpose.size(), 12);

    auto transposedPitches = DiatonicUtils::getTransposedPitchesInC(_xpose);

    // should transpose exact amount +1 1 semi
    for (int i = 0; i < 12; ++i) {
        const int trans = _xpose[i];
        assertGE(trans, amount - 1);
        assertLE(trans, amount + 1);
    }

#if 0
    int last = -1;
    for (int i = 0; i < 12; ++i) {
        int x = xpose[i];
        assertGE(x, 0);                 // valid entry
        assertGE(x, last);              // never decreasing
        last = x;

        assert(DiatonicUtils::isNoteInC(x));        // hard code to C for now

        // can assert that output is withing a semitone of input + trans
        assertGE(x, (i + amount - 1));
        assertLE(x, (i + amount + 1));
    }
#endif
}


static void testIsNoteInC()
{
    assert(DiatonicUtils::isNoteInC(DiatonicUtils::c));
    assert(!DiatonicUtils::isNoteInC(DiatonicUtils::c_));
    assert(DiatonicUtils::isNoteInC(DiatonicUtils::d));
    assert(!DiatonicUtils::isNoteInC(DiatonicUtils::d_));
    assert(DiatonicUtils::isNoteInC(DiatonicUtils::e));
    assert(DiatonicUtils::isNoteInC(DiatonicUtils::f));
    assert(!DiatonicUtils::isNoteInC(DiatonicUtils::f_));
    assert(DiatonicUtils::isNoteInC(DiatonicUtils::g));
    assert(!DiatonicUtils::isNoteInC(DiatonicUtils::g_));
    assert(DiatonicUtils::isNoteInC(DiatonicUtils::a));
    assert(!DiatonicUtils::isNoteInC(DiatonicUtils::a_));
    assert(DiatonicUtils::isNoteInC(DiatonicUtils::b));
}

static void testTransposeC_1(int amount)
{
    auto x = DiatonicUtils::getTransposeInC(amount);
    assertTransposeValidC(x, amount);
}


static void testTransposeC_1()
{
    testTransposeC_1(0);
  //  testTransposeC_1(2);
}

void testDiatonicUtils()
{
    testIsNoteInC();
    testTransposeC_1();

}