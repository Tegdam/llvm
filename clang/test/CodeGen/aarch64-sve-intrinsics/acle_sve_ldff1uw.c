// RUN: %clang_cc1 -D__ARM_FEATURE_SVE -triple aarch64-none-linux-gnu -target-feature +sve -fallow-half-arguments-and-returns -S -O1 -Werror -Wall -emit-llvm -o - %s | FileCheck %s

#include <arm_sve.h>

svint64_t test_svldff1uw_s64(svbool_t pg, const uint32_t *base)
{
  // CHECK-LABEL: test_svldff1uw_s64
  // CHECK: %[[PG:.*]] = call <vscale x 2 x i1> @llvm.aarch64.sve.convert.from.svbool.nxv2i1(<vscale x 16 x i1> %pg)
  // CHECK: %[[LOAD:.*]] = call <vscale x 2 x i32> @llvm.aarch64.sve.ldff1.nxv2i32(<vscale x 2 x i1> %[[PG]], i32* %base)
  // CHECK: %[[ZEXT:.*]] = zext <vscale x 2 x i32> %[[LOAD]] to <vscale x 2 x i64>
  // CHECK: ret <vscale x 2 x i64> %[[ZEXT]]
  return svldff1uw_s64(pg, base);
}

svuint64_t test_svldff1uw_u64(svbool_t pg, const uint32_t *base)
{
  // CHECK-LABEL: test_svldff1uw_u64
  // CHECK: %[[PG:.*]] = call <vscale x 2 x i1> @llvm.aarch64.sve.convert.from.svbool.nxv2i1(<vscale x 16 x i1> %pg)
  // CHECK: %[[LOAD:.*]] = call <vscale x 2 x i32> @llvm.aarch64.sve.ldff1.nxv2i32(<vscale x 2 x i1> %[[PG]], i32* %base)
  // CHECK: %[[ZEXT:.*]] = zext <vscale x 2 x i32> %[[LOAD]] to <vscale x 2 x i64>
  // CHECK: ret <vscale x 2 x i64> %[[ZEXT]]
  return svldff1uw_u64(pg, base);
}

svint64_t test_svldff1uw_vnum_s64(svbool_t pg, const uint32_t *base, int64_t vnum)
{
  // CHECK-LABEL: test_svldff1uw_vnum_s64
  // CHECK-DAG: %[[PG:.*]] = call <vscale x 2 x i1> @llvm.aarch64.sve.convert.from.svbool.nxv2i1(<vscale x 16 x i1> %pg)
  // CHECK-DAG: %[[BITCAST:.*]] = bitcast i32* %base to <vscale x 2 x i32>*
  // CHECK-DAG: %[[GEP:.*]] = getelementptr <vscale x 2 x i32>, <vscale x 2 x i32>* %[[BITCAST]], i64 %vnum, i64 0
  // CHECK: %[[LOAD:.*]] = call <vscale x 2 x i32> @llvm.aarch64.sve.ldff1.nxv2i32(<vscale x 2 x i1> %[[PG]], i32* %[[GEP]])
  // CHECK: %[[ZEXT:.*]] = zext <vscale x 2 x i32> %[[LOAD]] to <vscale x 2 x i64>
  // CHECK: ret <vscale x 2 x i64> %[[ZEXT]]
  return svldff1uw_vnum_s64(pg, base, vnum);
}

svuint64_t test_svldff1uw_vnum_u64(svbool_t pg, const uint32_t *base, int64_t vnum)
{
  // CHECK-LABEL: test_svldff1uw_vnum_u64
  // CHECK-DAG: %[[PG:.*]] = call <vscale x 2 x i1> @llvm.aarch64.sve.convert.from.svbool.nxv2i1(<vscale x 16 x i1> %pg)
  // CHECK-DAG: %[[BITCAST:.*]] = bitcast i32* %base to <vscale x 2 x i32>*
  // CHECK-DAG: %[[GEP:.*]] = getelementptr <vscale x 2 x i32>, <vscale x 2 x i32>* %[[BITCAST]], i64 %vnum, i64 0
  // CHECK: %[[LOAD:.*]] = call <vscale x 2 x i32> @llvm.aarch64.sve.ldff1.nxv2i32(<vscale x 2 x i1> %[[PG]], i32* %[[GEP]])
  // CHECK: %[[ZEXT:.*]] = zext <vscale x 2 x i32> %[[LOAD]] to <vscale x 2 x i64>
  // CHECK: ret <vscale x 2 x i64> %[[ZEXT]]
  return svldff1uw_vnum_u64(pg, base, vnum);
}
