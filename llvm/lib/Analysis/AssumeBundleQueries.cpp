//===- AssumeBundleQueries.cpp - tool to query assume bundles ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/AssumeBundleQueries.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PatternMatch.h"

using namespace llvm;
using namespace llvm::PatternMatch;

static bool bundleHasArgument(const CallBase::BundleOpInfo &BOI, unsigned Idx) {
  return BOI.End - BOI.Begin > Idx;
}

static Value *getValueFromBundleOpInfo(CallInst &Assume,
                                       const CallBase::BundleOpInfo &BOI,
                                       unsigned Idx) {
  assert(bundleHasArgument(BOI, Idx) && "index out of range");
  return (Assume.op_begin() + BOI.Begin + Idx)->get();
}

bool llvm::hasAttributeInAssume(CallInst &AssumeCI, Value *IsOn,
                                StringRef AttrName, uint64_t *ArgVal) {
  assert(isa<IntrinsicInst>(AssumeCI) &&
         "this function is intended to be used on llvm.assume");
  IntrinsicInst &Assume = cast<IntrinsicInst>(AssumeCI);
  assert(Assume.getIntrinsicID() == Intrinsic::assume &&
         "this function is intended to be used on llvm.assume");
  assert(Attribute::isExistingAttribute(AttrName) &&
         "this attribute doesn't exist");
  assert((ArgVal == nullptr || Attribute::doesAttrKindHaveArgument(
                                   Attribute::getAttrKindFromName(AttrName))) &&
         "requested value for an attribute that has no argument");
  if (Assume.bundle_op_infos().empty())
    return false;

  for (auto &BOI : Assume.bundle_op_infos()) {
    if (BOI.Tag->getKey() != AttrName)
      continue;
    if (IsOn && (BOI.End - BOI.Begin <= ABA_WasOn ||
                 IsOn != getValueFromBundleOpInfo(Assume, BOI, ABA_WasOn)))
      continue;
    if (ArgVal) {
      assert(BOI.End - BOI.Begin > ABA_Argument);
      *ArgVal =
          cast<ConstantInt>(getValueFromBundleOpInfo(Assume, BOI, ABA_Argument))
              ->getZExtValue();
    }
    return true;
  }
  return false;
}

void llvm::fillMapFromAssume(CallInst &AssumeCI, RetainedKnowledgeMap &Result) {
  IntrinsicInst &Assume = cast<IntrinsicInst>(AssumeCI);
  assert(Assume.getIntrinsicID() == Intrinsic::assume &&
         "this function is intended to be used on llvm.assume");
  for (auto &Bundles : Assume.bundle_op_infos()) {
    std::pair<Value *, Attribute::AttrKind> Key{
        nullptr, Attribute::getAttrKindFromName(Bundles.Tag->getKey())};
    if (bundleHasArgument(Bundles, ABA_WasOn))
      Key.first = getValueFromBundleOpInfo(Assume, Bundles, ABA_WasOn);

    if (Key.first == nullptr && Key.second == Attribute::None)
      continue;
    if (!bundleHasArgument(Bundles, ABA_Argument)) {
      Result[Key][&Assume] = {0, 0};
      continue;
    }
    unsigned Val = cast<ConstantInt>(
                       getValueFromBundleOpInfo(Assume, Bundles, ABA_Argument))
                       ->getZExtValue();
    auto Lookup = Result.find(Key);
    if (Lookup == Result.end() || !Lookup->second.count(&Assume)) {
      Result[Key][&Assume] = {Val, Val};
      continue;
    }
    Lookup->second[&Assume].Min = std::min(Val, Lookup->second[&Assume].Min);
    Lookup->second[&Assume].Max = std::max(Val, Lookup->second[&Assume].Max);
  }
}

RetainedKnowledge
llvm::getKnowledgeFromBundle(CallInst &Assume,
                             const CallBase::BundleOpInfo &BOI) {
  RetainedKnowledge Result;
  Result.AttrKind = Attribute::getAttrKindFromName(BOI.Tag->getKey());
  if (bundleHasArgument(BOI, ABA_WasOn))
    Result.WasOn = getValueFromBundleOpInfo(Assume, BOI, ABA_WasOn);
  if (BOI.End - BOI.Begin > ABA_Argument)
    Result.ArgValue =
        cast<ConstantInt>(getValueFromBundleOpInfo(Assume, BOI, ABA_Argument))
            ->getZExtValue();
  return Result;
}

RetainedKnowledge llvm::getKnowledgeFromOperandInAssume(CallInst &AssumeCI,
                                                        unsigned Idx) {
  IntrinsicInst &Assume = cast<IntrinsicInst>(AssumeCI);
  assert(Assume.getIntrinsicID() == Intrinsic::assume &&
         "this function is intended to be used on llvm.assume");
  CallBase::BundleOpInfo BOI = Assume.getBundleOpInfoForOperand(Idx);
  return getKnowledgeFromBundle(AssumeCI, BOI);
}

bool llvm::isAssumeWithEmptyBundle(CallInst &CI) {
  IntrinsicInst &Assume = cast<IntrinsicInst>(CI);
  assert(Assume.getIntrinsicID() == Intrinsic::assume &&
         "this function is intended to be used on llvm.assume");
  return none_of(Assume.bundle_op_infos(),
                 [](const CallBase::BundleOpInfo &BOI) {
                   return BOI.Tag->getKey() != IgnoreBundleTag;
                 });
}

static CallInst::BundleOpInfo *getBundleFromUse(const Use *U) {
  auto *Intr = dyn_cast<IntrinsicInst>(U->getUser());
  if (!match(U->getUser(),
             m_Intrinsic<Intrinsic::assume>(m_Unless(m_Specific(U->get())))))
    return nullptr;
  return &Intr->getBundleOpInfoForOperand(U->getOperandNo());
}

RetainedKnowledge
llvm::getKnowledgeFromUse(const Use *U,
                          ArrayRef<Attribute::AttrKind> AttrKinds) {
  CallInst::BundleOpInfo* Bundle = getBundleFromUse(U);
  if (!Bundle)
    return RetainedKnowledge::none();
  RetainedKnowledge RK =
      getKnowledgeFromBundle(*cast<CallInst>(U->getUser()), *Bundle);
  for (auto Attr : AttrKinds)
    if (Attr == RK.AttrKind)
      return RK;
  return RetainedKnowledge::none();
}

RetainedKnowledge
llvm::getKnowledgeForValue(const Value *V,
                           ArrayRef<Attribute::AttrKind> AttrKinds,
                           AssumptionCache *AC,
                           function_ref<bool(RetainedKnowledge, Instruction *,
                                             const CallBase::BundleOpInfo *)>
                               Filter) {
  if (AC) {
    for (AssumptionCache::ResultElem &Elem : AC->assumptionsFor(V)) {
      IntrinsicInst *II = cast_or_null<IntrinsicInst>(Elem.Assume);
      if (!II || Elem.Index == AssumptionCache::ExprResultIdx)
        continue;
      if (RetainedKnowledge RK = getKnowledgeFromBundle(
              *II, II->bundle_op_info_begin()[Elem.Index]))
        if (is_contained(AttrKinds, RK.AttrKind) &&
            Filter(RK, II, &II->bundle_op_info_begin()[Elem.Index]))
          return RK;
    }
    return RetainedKnowledge::none();
  }
  for (auto &U : V->uses()) {
    CallInst::BundleOpInfo* Bundle = getBundleFromUse(&U);
    if (!Bundle)
      continue;
    if (RetainedKnowledge RK =
            getKnowledgeFromBundle(*cast<CallInst>(U.getUser()), *Bundle))
      if (is_contained(AttrKinds, RK.AttrKind) &&
          Filter(RK, cast<Instruction>(U.getUser()), Bundle))
        return RK;
  }
  return RetainedKnowledge::none();
}

RetainedKnowledge llvm::getKnowledgeValidInContext(
    const Value *V, ArrayRef<Attribute::AttrKind> AttrKinds,
    const Instruction *CtxI, const DominatorTree *DT, AssumptionCache *AC) {
  return getKnowledgeForValue(V, AttrKinds, AC,
                              [&](auto, Instruction *I, auto) {
                                return isValidAssumeForContext(I, CtxI, DT);
                              });
}
