#include "gtest/gtest.h"
#include <iostream>
#include <stack>
#include <sstream>
#include <islutils/access_patterns.h>
#include <islutils/builders.h>
#include <islutils/ctx.h>
#include <islutils/locus.h>
#include <islutils/matchers.h>
#include <islutils/pet_wrapper.h>
#include <islutils/aff_op.h>
#include <islutils/cout_overloading.h>
#include <islutils/access.h>
#include <thread>
#include <fstream>

namespace conversion {

template < typename T > std::string to_string( const T& n ) {
        std::ostringstream stm ;
        stm << n ;
        return stm.str() ;
}

} // end namespace conversion


using util::ScopedCtx;

//enum class AccessType { read, write };

// lhs left hand side
// rhs righ hand side
// both lhs + rhs 
// not yet assigned

//enum class ArrayType { lhs, rhs, not_assigned };

//inline const std::string toString(ArrayType t) {
//  
//  switch(t) {
//    case ArrayType::lhs :
//      return "lhs";
//    case ArrayType::rhs :
//      return "rhs";
//    case ArrayType::not_assigned :
//      return "not assigned";
//    default :
//      return "Unknown";
//  }
//}


/// Parameters of the matrix multiplication operands.
///
/// Parameters, which describe access relations that represent operands of the
/// matrix multiplication.

class MatMulInfo {
  public:

    std::string A = "null";
    std::string B = "null";
    std::string ReadFromC = "null";
    std::string WriteToC = "null";

    int i = -1;
    int j = -1;
    int k = -1;
};

/// name holds the array name.
/// n_index the number of dimension(s) involved in the array
/// AccessType: is a read or a write access?
/// element_type: float, double or int?
/// extent holds constraints on the indices
/// access_map the access map.

class GpuArrayInfo {
  public:

    std::string name;
    unsigned n_index;
    std::string element_type;
    isl::set extent;
    std::vector<isl::map> accesses;

    //ArrayType type = ArrayType::not_assigned;
};

// payload for codegen.
struct payloadCodegen {
  std::vector<GpuArrayInfo> gpuArrays;
  std::vector<MatMulInfo> mmi;
};

static std::string getAccessName(isl::map m) {
  return m.range().get_tuple_id().get_name();
}

static std::string getAccessName(isl::set s) {
  return s.get_tuple_id().get_name();
}
/*
static unsigned getAccessIndexes(isl::map m) {
  return m.dim(isl::dim::out);
}
*/
static unsigned getAccessIndexes(isl::set s) {
  return s.dim(isl::dim::out);
}

// get array description from pet.
static isl::union_map applySchedule(isl::union_map schedule,
                                    isl::union_map accesses);

static std::vector<GpuArrayInfo> collectArrayInfo(Scop scop) {

  std::vector<GpuArrayInfo> res;

  isl::union_set arrays;
  isl::union_map accesses;

  std::vector<isl::set> arraysAsSet; 
  std::vector<isl::map> accessesAsMap;

  isl::union_map reads = scop.reads;
  accesses = reads;
  arrays = reads.range();
  isl::union_map writes = scop.mustWrites;
  accesses = accesses.unite(writes);

  arrays = arrays.unite(writes.range()); 

  arrays = arrays.coalesce();
  accesses = accesses.coalesce();
  arrays.foreach_set([&arraysAsSet](isl::set s) { 
    arraysAsSet.push_back(s); 
    return isl_stat_ok;
  });
  accesses.foreach_map([&accessesAsMap](isl::map m) { 
    accessesAsMap.push_back(m); 
    return isl_stat_ok;
  });
   
  for(size_t i = 0; i < arraysAsSet.size(); ++i) {
    GpuArrayInfo ga;
    ga.name = getAccessName(arraysAsSet[i]);
    for(int j = 0; j < scop.n_array; ++j) {
      std::string arrayName = getAccessName(scop.arrays[j].extent);
      if(arrayName.compare(ga.name) == 0) {
        ga.n_index = getAccessIndexes(arraysAsSet[i]);
        ga.element_type = scop.arrays[j].element_type;
        ga.extent = scop.arrays[j].extent;
        //for(size_t u = 0; u < accessesAsMap.size(); ++u) {
        //  if(getAccessName(accessesAsMap[u]).compare(ga.name)) {
        //    ga.accesses.push_back(accessesAsMap[u]);
        //  }
        //}
      } 
    }
    res.push_back(ga);
  }

  for(size_t i = 0; i < res.size(); ++i) {
    for(size_t j = 0; j < accessesAsMap.size(); ++j) {
      if(getAccessName(accessesAsMap[j]).compare(res[i].name) == 0) {
        res[i].accesses.push_back(accessesAsMap[j]);
      }
    }
  } 

  return res;
}

static isl::union_map applySchedule(isl::union_map schedule,
                                    isl::union_map accesses) {
  return accesses.apply_domain(schedule);
}

//static void printCudaHeader(std::string &s) {
//  s+= "/* Includes system */\n";
//  s+= "#include <stdio.h>\n";
//  s+= "#include <stdlib.h>\n\n";
//  s+= "/* Includes cuda */\n";
//  s+= "#include <cublas_v2.h>\n";
//  s+= "#include <cuda_runtime.h>\n";
//  s+= "#include <helper_cuda.h>\n";
//}

static std::string createIndent(int tab) {
  std::string result;
  for(int i = 0; i < tab; ++i) {
    result += " ";
  }
  return result;
}

static std::string macroCuBLASHandleInit(int tab) {
  std::string s;
  std::string indent = createIndent(tab);
  s+= "\n";
  s+= indent + "// First, create a cuBLAS handle:\n";
  s+= indent + "cublasStatus_t cublasStat = cublasCreate(&handle);\n";
  s+= indent + "if (cublasStat != CUBLAS_STATUS_SUCCESS) {\n";
  s+= indent + createIndent(2) + "return 0;\n";
  s+= indent + "}\n";
  s+= indent + "// Set the math mode to allow cuBLAS to use Tensor Cores:\n";
  s+= indent + "cublasStat = cublasSetMathMode(handle, CUBLAS_TENSOR_OP_MATH);";
  return s;
}

static std::string macroCuBLASHandleTearDown(int tab) {
  std::string s;
  std::string indent = createIndent(tab);
  s+= "\n";
  s+= indent + "/* Shutdown */\n";
  s+= indent + "cublasStat = cublasDestroy(handle);\n";
  return s;
}

/// print declaration for the device array.
static std::string declareDeviceArray(GpuArrayInfo g) {
  
  std::string result = "";
  result += g.element_type + " ";
  result += "*dev_";
  result += g.name;
  result += ";\n";
  return result;
}

/// does the gpu array need to be allocated on the device?
/// If it is a read-only scalar, then it will be passed
/// as argument to the function call.
static bool requireAllocation(GpuArrayInfo g) {
  
  if(g.n_index == 0) {
    return false;
  }
  return true;
}

/// print a declaration for the device array corresponding to
/// "array"
static std::string declareDeviceArrays(struct payloadCodegen *p, int tab) {
  
  std::string result = "\n";
  auto arrays = p->gpuArrays;

  for(size_t i = 0; i < arrays.size(); ++i) {
    // skip scalar accesses.
    if(requireAllocation(arrays[i]) == false) {
      continue;
    }
    else {
      result += createIndent(tab) + declareDeviceArray(arrays[i]);
    }
  }
  return result;
}

/// return arrays size
static std::string getNumberOfElementArray(GpuArrayInfo g) {
  
  isl::set extent = g.extent;
  std::vector<int> bounds;

  for(size_t i = 0; i < g.n_index; ++i) {

    auto allPoints = 
      isl::map::from_domain_and_range(extent, extent);
    isl::pw_aff min = allPoints.dim_min(i);
    isl::pw_aff max = allPoints.dim_max(i);

    EXPECT_TRUE(min.n_piece() == 1); 
    EXPECT_TRUE(max.n_piece() == 1);

    isl::val min_val;
    isl::val max_val;
 
    min.foreach_piece([&](isl::set s, isl::aff aff) {
      min_val = aff.get_constant_val();
      return isl_stat_ok; 
    });
    max.foreach_piece([&](isl::set s, isl::aff aff) {
      max_val = aff.get_constant_val();
      return isl_stat_ok;
    });

    max_val = max_val.sub(min_val);
    int bound = atoi(max_val.to_str().c_str());
    bounds.push_back(bound);
  }

  unsigned numberOfElement = 1; 
  for(size_t i = 0; i < bounds.size(); ++i) {
    numberOfElement *= static_cast<unsigned>(bounds[i]);
  }

  return conversion::to_string(numberOfElement);
}

/// print arrays allocation
static std::string allocateDeviceArrays(struct payloadCodegen *p, int tab) {

  std::string result = "\n";
  auto arrays = p->gpuArrays;

  for(size_t i = 0; i < arrays.size(); ++i) {
    //skip sclar accesses.
    if(requireAllocation(arrays[i]) == false) {
      continue;
    }
    else {
      result += createIndent(tab) + "if (cudaMalloc(reinterpret_cast<void **>(&dev_"
             + arrays[i].name + ")" + ", " + "sizeof(dev_" + arrays[i].name 
             + "[0]) * " + getNumberOfElementArray(arrays[i]) + ") != cudaSuccess) {\n";
      result += createIndent(tab + 2) + "return 0;\n";
      result += createIndent(tab) + "}\n";
    }
  }
  return result;
}

/// print stmt for copying to device.
static std::string copyToDeviceArrays(struct payloadCodegen *p, int tab) {

  auto arrays = p->gpuArrays;
  std::string result = "\n";

  for(size_t i = 0; i < arrays.size(); ++i) {
    if(requireAllocation(arrays[i]) == false) {
      continue;
    } 
    else {
      result += createIndent(tab) + "cublasStat = cublasSetVector(";
      result += getNumberOfElementArray(arrays[i]);
      result += ", ";
      result += "sizeof(" + arrays[i].name + "[0]" + ")";
      result += ", ";
      result += arrays[i].name;
      result += ", ";
      result += "1";
      result += ", ";
      result += "dev_" + arrays[i].name;
      result += ", ";
      result += "1);\n";
    }
  }

  return result;
}

/// print code for initializing the device for the execution.
/// This includes declaring locally defined variables as well as
/// declaring and allocating the required copies of arrays on device.
static std::string copyToDevice(struct payloadCodegen *p, int tab) {

  std::string result = "\n";
  result += declareDeviceArrays(p, tab);
  result += allocateDeviceArrays(p, tab);
  result += copyToDeviceArrays(p, tab);
  return result;
}

static std::string copyFromDeviceArray(struct payloadCodegen *p, int tab) {

  auto arrays = p->gpuArrays;
  std::vector<MatMulInfo> mmiBatched = p->mmi;
  std::string result;

  // always return the last write access 
  // may not be always correct.
  MatMulInfo mmi = mmiBatched[0];
  for(size_t i = 0; i < arrays.size(); ++i) {
    if(arrays[i].name.compare(mmi.WriteToC) == 0) {
      result += createIndent(tab) + "cublasStat = cublasGetVector(";
      result += getNumberOfElementArray(arrays[i]);
      result += ", ";
      result += "sizeof(" + arrays[i].name + "[0]" + ")";
      result += ", ";
      result += "dev_" + arrays[i].name;
      result += ", ";
      result += "1";
      result += ", ";
      result += arrays[i].name;
      result += ", ";
      result += "1);\n";
    }
  }
  return result;
}

static std::string copyFromDevice(struct payloadCodegen *p, int tab) {

  std::string result = "\n";
  result += copyFromDeviceArray(p, tab);
  return result;
} 

/// Add nodes for copying to and from the device after node "node".
//static isl::schedule_node addInitAndClearDevice(isl::schedule_node node) {
//
//  isl::space space;
//  isl::union_set domain;
//  isl::schedule_node graft;
//
//  space = isl::space(node.get_ctx(), 0, 0);
//  space = space.set_tuple_name(isl::dim::set, "copy_to_device");
//  domain = isl::union_set(isl::set::universe(space));
//  graft = isl::schedule_node::from_domain(domain);
//
//  node = node.graft_before(graft);
//
//  space = isl::space(node.get_ctx(), 0, 0);
//  space = space.set_tuple_name(isl::dim::set, "copy_from_device");
//  domain = isl::union_set(isl::set::universe(space));
//  graft = isl::schedule_node::from_domain(domain);
//
//  node = node.graft_after(graft);
//  
//  return node;
//}

/// Add nodes for copying to the device before "node"
static isl::schedule_node addCopyToDevice(isl::schedule_node node) {

  isl::space space;
  isl::union_set domain;
  isl::schedule_node graft;

  space = isl::space(node.get_ctx(), 0, 0);
  space = space.set_tuple_name(isl::dim::set, "copy_to_device");
  domain = isl::union_set(isl::set::universe(space));
  graft = isl::schedule_node::from_domain(domain);

  node = node.graft_before(graft);

  return node;
}

/// Add nodes for copying from the device after "node"
static isl::schedule_node addCopyFromDevice(isl::schedule_node node) {

  isl::space space;
  isl::union_set domain;
  isl::schedule_node graft;

  space = isl::space(node.get_ctx(), 0, 0);
  space = space.set_tuple_name(isl::dim::set, "copy_from_device");
  domain = isl::union_set(isl::set::universe(space));
  graft = isl::schedule_node::from_domain(domain);

  node = node.graft_after(graft);

  return node;
}


/// Add nodes to delimiting a kernel that will be swapped with a function
/// call to cuBLAS.
static isl::schedule_node addKernelBoundaries(isl::schedule_node node) {

  isl::space space;
  isl::union_set domain;
  isl::schedule_node graft;

  space = isl::space(node.get_ctx(), 0, 0);
  space = space.set_tuple_name(isl::dim::set, "kernel_start");
  domain = isl::union_set(isl::set::universe(space));
  graft = isl::schedule_node::from_domain(domain);

  node = node.graft_before(graft);

  space = isl::space(node.get_ctx(), 0, 0);
  space = space.set_tuple_name(isl::dim::set, "kernel_end");
  domain = isl::union_set(isl::set::universe(space));
  graft = isl::schedule_node::from_domain(domain);

  node = node.graft_after(graft);

  return node;
}

/// Add node for inserting cuBLAS handles.
//static isl::schedule_node addCuBLASHandles(isl::schedule_node node) {
//
//  isl::space space;
//  isl::union_set domain;
//  isl::schedule_node graft;
//
//  space = isl::space(node.get_ctx(), 0, 0);
//  space = space.set_tuple_name(isl::dim::set, "cuBLAS_manage_init");
//  domain = isl::union_set(isl::set::universe(space));
//  graft = isl::schedule_node::from_domain(domain);
//
//  node = node.graft_before(graft);
//
//  space = isl::space(node.get_ctx(), 0, 0);
//  space = space.set_tuple_name(isl::dim::set, "cuBLAS_tear_down");
//  domain = isl::union_set(isl::set::universe(space));
//  graft = isl::schedule_node::from_domain(domain);
//
//  node = node.graft_after(graft);
//  
//  return node;
//}

/// Add node for inserting cuBLAS handles - start-up
static isl::schedule_node addCuBLASHandleStartUp(isl::schedule_node node) {

  isl::space space;
  isl::union_set domain;
  isl::schedule_node graft;

  space = isl::space(node.get_ctx(), 0, 0);
  space = space.set_tuple_name(isl::dim::set, "cuBLAS_manage_init");
  domain = isl::union_set(isl::set::universe(space));
  graft = isl::schedule_node::from_domain(domain);

  node = node.graft_before(graft);
  return node;
}

/// Add node for inserting cuBLAS handles - tear-down
static isl::schedule_node addCuBLASHandleTearDown(isl::schedule_node node) {
  
  isl::space space;
  isl::union_set domain;
  isl::schedule_node graft;

  space = isl::space(node.get_ctx(), 0, 0);
  space = space.set_tuple_name(isl::dim::set, "cuBLAS_tear_down");
  domain = isl::union_set(isl::set::universe(space));
  graft = isl::schedule_node::from_domain(domain);

  node = node.graft_after(graft);
  return node;
}

static std::string codeGenGPU(isl::ast_build astBuild, isl::ast_node node,
                              pet_stmt *stmt, void *user) {

  auto t = static_cast<struct payloadCodegen*>(user);
  
  auto schedule = astBuild.get_schedule();
  auto name = isl::set(schedule.domain()).get_tuple_id().get_name();
  if(name == "cuBLAS_manage_init") {
    return macroCuBLASHandleInit(2);
  }
  if(name == "cuBLAS_tear_down") {
    return macroCuBLASHandleTearDown(2);
  }
  if(name == "kernel_start") {
    return "kernel_start";
  }
  if(name == "kernel_end") {
    return "kernel_end";
  }
  if(name == "copy_from_device") {
    return copyFromDevice(t,2);
  }
  if(name == "copy_to_device") {
    return copyToDevice(t,2);
  }
  else {
    return "I will be removed :(";
  }
}

// mark the array with id "t" with the mark "m"
//void assignTypeArray(std::string t, ArrayType m, std::vector<GpuArrayInfo> &gv) {
//
//  for(size_t i = 0; i < gv.size(); ++i) {
//    if(gv[i].name.compare(t) !=0) {
//      continue;
//    }
//    else {
//      if(gv[i].type == ArrayType::not_assigned) {
//        gv[i].type = ArrayType::lhs;
//      }
//      else {
//        std::cout << "not defined yet" << std::endl;
//        ASSERT_TRUE(0);
//      }
//    }
//  }
//}


std::pair<isl::map, bool> 
findAccess(std::vector<GpuArrayInfo> &gv, int x, int y, isl::union_map s) {

  std::vector<isl::map> allAccesses;
  for(size_t i = 0; i < gv.size(); ++i) {
    for(size_t j = 0; j < gv[i].accesses.size(); ++j) {
      allAccesses.push_back(gv[i].accesses[j]);
    }
  }

  std::vector<int> indexesDiscovered;

  for(size_t i = 0; i < allAccesses.size(); ++i) {
    isl::union_map scheduledAccess = isl::union_map(allAccesses[i]);
    scheduledAccess.apply_domain(s);
    isl::map m = isl::map::from_union_map(scheduledAccess);
    isl::pw_multi_aff multiAff = isl::pw_multi_aff::from_map(m);
    
    if(m.dim(isl::dim::out) != 2) {
      continue;
    }
    if(m.dim(isl::dim::in) != 3) {
      continue;
    }
    // skip if sched and access to not belong to the same 
    // stmt (not a good implementation).
    isl::map schedAsMap = isl::map::from_union_map(s);
    if(m.domain().unwrap().domain().get_tuple_id().get_name()
       .compare(schedAsMap.get_tuple_id(isl::dim::in).get_name()) != 0) {
      continue;
    }

    for(size_t ot = 0; ot < m.dim(isl::dim::out); ++ot) {
      isl::pw_aff pwa = multiAff.get_pw_aff(ot);
      pwa.foreach_piece([&](isl::set s, isl::aff a){
        for(size_t in = 0; in < m.dim(isl::dim::in); ++in) {
          isl::val v = a.get_coefficient_val(isl::dim::in, in);
          if(v.is_one()) {
            indexesDiscovered.push_back(in);
          }
        }
        return isl_stat_ok;
      });
    }

    if(indexesDiscovered[0] == x && indexesDiscovered[1] == y) {
      return std::make_pair(m, true);
    }
    indexesDiscovered.erase(indexesDiscovered.begin(), 
                            indexesDiscovered.end());
  }

  return std::make_pair(nullptr, false);
}

void fillMMI(std::vector<GpuArrayInfo> &gv, MatMulInfo &MMI, isl::union_map sched) {
 
  isl::map readFromC = findAccess(gv, MMI.i, MMI.j, sched).first;
  
  isl::map A = findAccess(gv, MMI.i, MMI.k, sched).first;

  isl::map B = findAccess(gv, MMI.k, MMI.j, sched).first;

  MMI.ReadFromC = getAccessName(readFromC);
  MMI.A = getAccessName(A);
  MMI.B = getAccessName(B);
}

std::vector<int> 
getRowsNumber(std::vector<std::string> ids, std::vector<GpuArrayInfo> gp) {
  
  std::vector<int> rows;
  for(size_t j = 0; j < ids.size(); ++j) {

    for(size_t i = 0; i < gp.size(); ++i) {
      if(gp[i].name.compare(ids[j]) != 0) {
        continue;
      }
      else {
 
        isl::set extent = gp[i].extent;
        EXPECT_TRUE(extent.dim(isl::dim::out) == 2);

        auto allPoints =
          isl::map::from_domain_and_range(extent, extent);
        isl::pw_aff min = allPoints.dim_min(0);
        isl::pw_aff max = allPoints.dim_max(0);

        EXPECT_TRUE(min.n_piece() == 1);
        EXPECT_TRUE(max.n_piece() == 1);

        isl::val min_val;
        isl::val max_val;

        min.foreach_piece([&](isl::set s, isl::aff aff) {
          min_val = aff.get_constant_val();
          return isl_stat_ok;
        });
        max.foreach_piece([&](isl::set s, isl::aff aff) {
          max_val = aff.get_constant_val();
          return isl_stat_ok;
        });

        max_val = max_val.sub(min_val);
        rows.push_back(atoi(max_val.to_str().c_str()));
      }
    }
  }
  return rows;   
}

std::vector<int> 
getColumnsNumber(std::vector<std::string> ids, std::vector<GpuArrayInfo> gp) {
  
  std::vector<int> columns;
  for(size_t j = 0; j < ids.size(); ++j) {

    for(size_t i = 0; i < gp.size(); ++i) {
      if(gp[i].name.compare(ids[j])  != 0) {
        continue;
      }
      else {
        isl::set extent = gp[i].extent;
        EXPECT_TRUE(extent.dim(isl::dim::out) == 2);
   
        auto allPoints =
          isl::map::from_domain_and_range(extent, extent);
        isl::pw_aff min = allPoints.dim_min(1);
        isl::pw_aff max = allPoints.dim_max(1);

        isl::val min_val;
        isl::val max_val;

        min.foreach_piece([&](isl::set s, isl::aff aff) {
          min_val = aff.get_constant_val();
          return isl_stat_ok;
        });
        max.foreach_piece([&](isl::set s, isl::aff aff) {
          max_val = aff.get_constant_val();
          return isl_stat_ok;
        });

        max_val = max_val.sub(min_val);
        columns.push_back(atoi(max_val.to_str().c_str()));
      }
    }
  }
  return columns;
}

std::vector<int> 
getLeadingDimension(std::vector<std::string> ids, std::vector<GpuArrayInfo> gp) {
  return getRowsNumber(ids, gp);
}

std::vector<std::string> getConstantInnerMostLoop(std::vector<GpuArrayInfo> gp) {
  
  std::vector<std::string> res;
  for(size_t i = 0; i < gp.size(); ++i) {
    if(gp[i].n_index != 0) {
      continue;
    }
    if(gp[i].accesses[0].dim(isl::dim::in) != 3) {
      continue;
    }
    else {
      res.push_back(gp[i].name);
    }
  }
  return res;
}

std::vector<std::string> getConstantInitStmt(std::vector<GpuArrayInfo> gp) {
  
  std::vector<std::string> res;
  for(size_t i = 0; i < gp.size(); ++i) {
    if(gp[i].n_index != 0) { 
      continue;
    }
    if(gp[i].accesses[0].dim(isl::dim::in) != 2) {
      continue;
    }
    else {
      res.push_back(gp[i].name);
    }
  }
  return res;
}

std::string insertCallToCUBALS(std::string c, struct payloadCodegen *p) {

  auto arrays = p->gpuArrays;
  std::vector<MatMulInfo> MMIBatched = p->mmi;

  // contains id for "A" array(s)
  std::vector<std::string> aArrays;
  for(size_t i = 0; i < MMIBatched.size(); ++i) {
    aArrays.push_back(MMIBatched[i].A);
  }
  // contains id for "ReadFromC" array(s)
  std::vector<std::string> readFromCArrays;
  for(size_t i = 0; i < MMIBatched.size(); ++i) {
    readFromCArrays.push_back(MMIBatched[i].ReadFromC);
  }
  // contains id for "B" array(s)
  std::vector<std::string> bArrays;
  for(size_t i = 0; i < MMIBatched.size(); ++i) {
    bArrays.push_back(MMIBatched[i].B);
  }
  // contains id for "WriteToC" array(s)
  std::vector<std::string> writeToCArrays;
  for(size_t i = 0; i < MMIBatched.size(); ++i) {
    writeToCArrays.push_back(MMIBatched[i].WriteToC);
  }

  // m: number of rows of matrix op(A) and op(C)
  std::vector<int> mAs = getRowsNumber(aArrays, arrays);
  std::vector<int> mCs = getRowsNumber(readFromCArrays, arrays);
  EXPECT_TRUE(mAs == mCs);
  EXPECT_TRUE(
    std::adjacent_find(mAs.begin(), mAs.end(), std::not_equal_to<int>()) == mAs.end());
  EXPECT_TRUE(
    std::adjacent_find(mCs.begin(), mCs.end(), std::not_equal_to<int>()) == mCs.end());

  // n: number of columns of matrix op(B) and op(C)
  std::vector<int> nBs = getColumnsNumber(bArrays, arrays);
  std::vector<int> nCs = getColumnsNumber(readFromCArrays, arrays);
  EXPECT_TRUE(nBs == nCs);
  EXPECT_TRUE(
    std::adjacent_find(nBs.begin(), nBs.end(), std::not_equal_to<int>()) == nBs.end());
  EXPECT_TRUE(
    std::adjacent_find(nCs.begin(), nCs.end(), std::not_equal_to<int>()) == nCs.end());

  // k: number of columns of matrix op(B) and op(A)
  std::vector<int> kBs = getColumnsNumber(bArrays, arrays);
  std::vector<int> kAs = getColumnsNumber(aArrays, arrays);
  EXPECT_TRUE(kBs == kAs);
  EXPECT_TRUE(
    std::adjacent_find(kBs.begin(), kBs.end(), std::not_equal_to<int>()) == kBs.end());
  EXPECT_TRUE(
    std::adjacent_find(kAs.begin(), kAs.end(), std::not_equal_to<int>()) == kAs.end());

  // alpha scaling factor for A*B
  std::vector<std::string> alphas = getConstantInnerMostLoop(arrays);
  // beta scaling factor for C
  std::vector<std::string> betas = getConstantInitStmt(arrays);

  // lda leading dimension(s) of two-dimensional array used to store the matrix A
  std::vector<int> ldas = getLeadingDimension(aArrays, arrays);
  EXPECT_TRUE(
    std::adjacent_find(ldas.begin(), ldas.end(), std::not_equal_to<int>()) == ldas.end());

  // ldb leading dimension(s) of two-dimensional array used to store the matrix B
  std::vector<int> ldbs = getLeadingDimension(bArrays, arrays);
  EXPECT_TRUE(
    std::adjacent_find(ldbs.begin(), ldbs.end(), std::not_equal_to<int>()) == ldbs.end());

  // ldc leading dimension(s) of two-dimensional array used to store the matrix C
  std::vector<int> ldcs = getLeadingDimension(readFromCArrays, arrays);
  EXPECT_TRUE(
    std::adjacent_find(ldcs.begin(), ldcs.end(), std::not_equal_to<int>()) == ldcs.end());

  std::string fCall;

  if(MMIBatched.size() == 1) {

    fCall += createIndent(2) + "cublasStat = \n ";
    fCall += createIndent(4) + "cublasGemmEx(handle,\n"; 
    fCall += createIndent(18) + "CUBLAS_OP_N, \n";
    fCall += createIndent(18) + "CUBLAS_OP_N, \n";
    fCall += createIndent(18) + conversion::to_string(mAs[0]) + ", \n";
    fCall += createIndent(18) + conversion::to_string(nBs[0]) + ", \n";
    fCall += createIndent(18) + conversion::to_string(kBs[0]) + ", \n";
    if(alphas.empty()) {
      fCall += createIndent(18) + "1" + ", \n";
    }
    else {
      fCall += createIndent(18) + "&" + alphas[0] + ", \n";
    }
    fCall += createIndent(18) + MMIBatched[0].A + ", \n";
    fCall += createIndent(18) + "CUDA_R_16F" + ", \n";
    fCall += createIndent(18) + conversion::to_string(ldas[0]) + ", \n";
    fCall += createIndent(18) + MMIBatched[0].B + ", \n";
    fCall += createIndent(18) + "CUDA_R_16F" + ", \n";
    fCall += createIndent(18) + conversion::to_string(ldbs[0]) + ", \n";
    if(betas.empty()) {
      fCall += createIndent(18) + "1" + ", \n";
    }
    else {
      fCall += createIndent(18) + "&" + betas[0] + ", \n";
    }
    fCall += createIndent(18) + MMIBatched[0].WriteToC + ", \n";
    fCall += createIndent(18) + "CUDA_R_16F" + ", \n";
    fCall += createIndent(18) + conversion::to_string(ldcs[0]) + ", \n";
    fCall += createIndent(18) + "CUDA_R_32F" + ", \n";
    fCall += createIndent(18) + "CUBLAS_GEMM_DEFAULT);";  
  }
  else {
    std::string sizeAsString = conversion::to_string(MMIBatched.size());

    // array of pointers to "aArray"
    fCall += "const void *Aarray[" + sizeAsString + "] = {";
    for(size_t i = 0; i < aArrays.size(); ++i) {
      if(i == aArrays.size() -1) {
        fCall += aArrays[i] + "};" + "\n";
      }
      else {
        fCall += aArrays[i] + ", ";
      }
    }

    // array of pointers to "bArray"
    fCall += createIndent(2) + "const void *Barray[" + sizeAsString + "] = {";
    for(size_t i = 0; i < bArrays.size(); ++i) {
      if(i == bArrays.size() -1) {
        fCall += bArrays[i] + "};" + "\n";
      }
      else {
        fCall += bArrays[i] + ", ";
      }
    }

    // array of pointers to "writeToCArrays"
    fCall += createIndent(2) + "const void *Carray[" + sizeAsString + "] = {";
    for(size_t i = 0; i < writeToCArrays.size(); ++i) {
      if(i == writeToCArrays.size() -1) {
        fCall += writeToCArrays[i] + "};" + "\n";
      }
      else {
        fCall += writeToCArrays[i] + ", ";
      }
    }

    fCall += createIndent(2) + "cublasStat = \n";
    fCall += createIndent(4) + "cublasGemmBatchedEx(handle,\n";
    fCall += createIndent(18) + "CUBLAS_OP_N, \n";
    fCall += createIndent(18) + "CUBLAS_OP_N, \n";
    // the batch is considered to be uniform i.e. all instances
    // have the same dimension (m, n, k)
    fCall += createIndent(18) + conversion::to_string(mAs[0]) + ", \n";
    fCall += createIndent(18) + conversion::to_string(nBs[0]) + ", \n";
    fCall += createIndent(18) + conversion::to_string(kBs[0]) + ", \n";
    if(alphas.empty()) {
      fCall += createIndent(18) + "1" + ", \n";
    }
    else {
      fCall += createIndent(18) + "&" + alphas[0] + ", \n";
    }
    fCall += createIndent(18) + "Aarray,\n";
    fCall += createIndent(18) + "CUDA_R_16F" + ", \n"; 
    fCall += createIndent(18) + conversion::to_string(ldas[0]) + ", \n";
    fCall += createIndent(18) + "Barray,\n";
    fCall += createIndent(18) + "CUDA_R_16F" + ", \n";
    fCall += createIndent(18) + conversion::to_string(ldbs[0]) + ", \n";
    if(betas.empty()) {
      fCall += createIndent(18) + "1" + ", \n";
    }
    else {
      fCall += createIndent(18) + "&" + betas[0] + ", \n";
    }
    fCall += createIndent(18) + "Carray,\n";
    fCall += createIndent(18) + "CUDA_R_16F" + ", \n";
    fCall += createIndent(18) + conversion::to_string(ldbs[0]) + ", \n";
    fCall += createIndent(18) + conversion::to_string(MMIBatched.size()) + ", \n";
    fCall += createIndent(18) + "CUDA_R_32F" + ", \n"; 
    fCall += createIndent(18) + "CUBLAS_GEMM_DEFAULT);";
  }

  std::string startK = "kernel_start";
  std::string endK = "kernel_end";
  c.replace(c.find(startK),
            c.find(endK) - c.find(startK) + endK.size(), fCall);
  return c;
}


// forward declaration for test codeGenerationGPUs.
isl::union_map addRangeId(isl::union_map umap, const std::string &tag);
static inline isl::schedule_node
rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement);

TEST(Transformers, codeGenPayload) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = 
    pet::Scop::parseFile(ctx, "inputs/gemm.c");
  auto scop =
    petScop.getScop();
  isl::schedule_node root = scop.schedule.get_root();
  petScop.schedule() = root.get_schedule();
  //std::cout << petScop.codegenPayload() << std::endl;
}

std::vector<isl::schedule_node>
findPatterns(const matchers::ScheduleNodeMatcher &m,
             isl::schedule_node root) {

  std::vector<isl::schedule_node> rootMatched;
  std::stack<isl::schedule_node> nodeStack;
  nodeStack.push(root);

  while(nodeStack.empty() == false) {
    root = nodeStack.top();
    nodeStack.pop();

    if(matchers::ScheduleNodeMatcher::isMatching(m, root)) {
      rootMatched.push_back(root);
    }
  
    size_t n_children =
      static_cast<size_t>(isl_schedule_node_n_children(root.get()));
    for(size_t i = 0; i < n_children; i++) {
      nodeStack.push(root.child(i));
    }
  }

  return rootMatched;
}


TEST(Transformers, codeGenerationGPUs_matcher) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/gemm.c");
  auto scop =
    petScop.getScop();
  isl::schedule_node root = scop.schedule.get_root();
 
  using namespace matchers;

  auto is1Dim = [&](isl::schedule_node band) {
    
    return true;
  };

  auto matcher =
    band(is1Dim,
      band(is1Dim,
        sequence(
          filter(leaf()),
          filter(band(is1Dim,leaf())))));

  auto gemm = findPatterns(matcher, root);
  EXPECT_TRUE(gemm.size() == 1);
  
  scop = pet::Scop::parseFile(ctx, "inputs/2mm.c").getScop();
  root = scop.schedule.get_root();
  auto twoMM = findPatterns(matcher, root);
  EXPECT_TRUE(twoMM.size() == 2);

  scop = pet::Scop::parseFile(ctx, "inputs/3mm.c").getScop();
  root = scop.schedule.get_root();
  auto threeMM = findPatterns(matcher, root);
  EXPECT_TRUE(threeMM.size() == 3);
}

/// DFS function to apply a function "fun" repeatedly.
isl::schedule_node
applyRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  std::function<isl::schedule_node(isl::schedule_node)> func) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = func(node);
  }
  return node;
}

isl::schedule_node
applyDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             std::function<isl::schedule_node(isl::schedule_node)> func) {
  node = applyRepeatedly(node, pattern, func);
  for (int i = 0; i < node.n_children(); ++i) {
    node = applyDFSPreorderRepeatedly(node.child(i), pattern, func)
               .parent();
  }
  return node;
}

isl::schedule_node
applyOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            std::function<isl::schedule_node(isl::schedule_node)> func) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = func(node);
  }
  return node;
}

isl::schedule_node
applyDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       std::function<isl::schedule_node(isl::schedule_node)> func) {
  node = applyOnce(node, pattern, func);
  for (int i = 0; i < node.n_children(); ++i) {
    node = applyDFSPreorderOnce(node.child(i), pattern, func).parent();
  }
  return node;
}

TEST(Transformers, codeGenerationGPUs_flow_doubleGEMM) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/2mm.c");
  auto scop = petScop.getScop();

  isl::schedule_node root = scop.schedule.get_root();
  isl::union_map reads = scop.reads.curry();
  isl::union_map writes = scop.mustWrites.curry();

  // matmul matcher and call back function.
  // the callback should check that every band is 
  // single dimensional. We also check for a matmul
  // with initialization statement.
  using namespace matchers;
  auto is1Dim = [&] (isl::schedule_node band) {
    return true;
  };
  auto matcherGEMM =
    band(is1Dim,
      band(is1Dim,
        sequence(
          filter(leaf()),
          filter(band(is1Dim, leaf())))));

  std::vector<isl::schedule_node> matches = findPatterns(matcherGEMM, root);
  EXPECT_TRUE(matches.size() == 2);

  // collect global array info from the entire scop.
  std::vector<GpuArrayInfo> arrayInfo = collectArrayInfo(scop);

  // for each matmul detected we obtain the accesses that belong to 
  // the core statement. On these accesses we run the matchers to make
  // sure the pattern is the matmul one.
  // Currently, we are not looking at the stride, but this is a trivial 
  // extension of the current flow, since the stride can be checked with
  // the matchers as well.
  std::vector<MatMulInfo> MMIBatched;
  for(size_t matmulIndex = 0; matmulIndex < matches.size(); ++matmulIndex) {
    isl::schedule_node leafInit = 
      matches[matmulIndex].child(0).child(0).child(0).child(0);
    isl::schedule_node leafCore = 
      matches[matmulIndex].child(0).child(0).child(1).child(0).child(0);
    isl::union_map prefixSchedule = leafInit.get_prefix_schedule_union_map();
    isl::union_map accessesLeafInitStmtR = applySchedule(prefixSchedule, reads);
    isl::union_map accessesLeafInitStmtW = applySchedule(prefixSchedule, writes);
    prefixSchedule = leafCore.get_prefix_schedule_union_map();
    isl::union_map accessesLeafCoreStmtR = applySchedule(prefixSchedule, reads);
    isl::union_map accessesLeafCoreStmtW = applySchedule(prefixSchedule, writes);
    accessesLeafCoreStmtR = accessesLeafCoreStmtR.subtract(accessesLeafInitStmtR);
    accessesLeafCoreStmtW = accessesLeafCoreStmtW.subtract(accessesLeafInitStmtW);

    using namespace matchers;
    auto _i = placeholder(ctx);
    auto _j = placeholder(ctx);
    auto _k = placeholder(ctx);
    auto _ii = placeholder(ctx);
    auto _jj = placeholder(ctx);

    auto _A = arrayPlaceholder();
    auto _B = arrayPlaceholder();
    auto _C = arrayPlaceholder();

    auto psRead =
      allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
    auto readMatches = match(accessesLeafCoreStmtR, psRead);
    ASSERT_EQ(readMatches.size(), 1u);
    auto psWrite = allOf(access(_A, _ii, _jj));
    auto writeMatches = match(accessesLeafCoreStmtW, psWrite);
    ASSERT_EQ(writeMatches.size(), 1u);

    // check index for read and write are equal
    ASSERT_TRUE(writeMatches[0][_ii].payload().inputDimPos_ ==
                readMatches[0][_i].payload().inputDimPos_);
    ASSERT_TRUE(writeMatches[0][_jj].payload().inputDimPos_ ==
                readMatches[0][_j].payload().inputDimPos_);

    std::vector<isl::space> v_space = writeMatches[0][_ii].candidateSpaces();
    ASSERT_TRUE(v_space.size() == 1);

    // fill the MMI structure.
    MatMulInfo MMI;
    MMI.WriteToC =
      v_space[0].range().unwrap().range().get_tuple_name(isl::dim::out);
    MMI.i = writeMatches[0][_ii].payload().inputDimPos_;
    MMI.j = writeMatches[0][_jj].payload().inputDimPos_;
    MMI.k = readMatches[0][_k].payload().inputDimPos_;
    fillMMI(arrayInfo, MMI, prefixSchedule.intersect_domain(leafCore.get_domain()));
    MMIBatched.push_back(MMI);
  }

  ASSERT_TRUE(MMIBatched[0].WriteToC.compare("D") == 0);
  ASSERT_TRUE(MMIBatched[0].ReadFromC.compare("D") == 0);
  ASSERT_TRUE(MMIBatched[0].A.compare("tmp") == 0);
  ASSERT_TRUE(MMIBatched[0].B.compare("C") == 0);
  ASSERT_TRUE(MMIBatched[1].WriteToC.compare("tmp") == 0);
  ASSERT_TRUE(MMIBatched[1].ReadFromC.compare("tmp") == 0);
  ASSERT_TRUE(MMIBatched[1].A.compare("A") == 0);
  ASSERT_TRUE(MMIBatched[1].B.compare("B") == 0);

  // insert marking node for init, clean and copy to/from device.
  root = matches[0].parent().parent();
  root = addCuBLASHandleStartUp(root);
  root = addCuBLASHandleTearDown(root);
  root = addCopyToDevice(root);
  root = addCopyFromDevice(root);
  root = addKernelBoundaries(root); 
  petScop.schedule() = root.get_schedule();
  //static_cast<isl::schedule>(petScop.schedule()).dump();

  payloadCodegen p = {arrayInfo, MMIBatched};

  //std::string codeGen = petScop.codegenPayload(codeGenGPU, &p);
  //codeGen = insertCallToCUBALS(codeGen, &p);
  //std::cout << codeGen << std::endl;

}

TEST(Transformers, codeGenerationGPUs_flow_singleGEMM) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/gemm.c");
  auto scop =
    petScop.getScop();
  isl::schedule_node root = scop.schedule.get_root();
  
  isl::union_map reads = scop.reads.curry();
  isl::union_map writes = scop.mustWrites.curry();
  std::vector<GpuArrayInfo> arrayInfo = collectArrayInfo(scop);

  ASSERT_TRUE(arrayInfo.size() == 5);

  // leaf init stmt
  root = root.child(0).child(0).child(0)
             .child(0).child(0);
  isl::union_map prefixSchedule = root.get_prefix_schedule_union_map();
  isl::union_map accessesLeafInitStmtR = applySchedule(prefixSchedule, reads);
  isl::union_map accessesLeafInitStmtW = applySchedule(prefixSchedule, writes);

  root = root.root();
  // leaf core stmt
  root = root.child(0).child(0).child(0)
             .child(1).child(0).child(0);
  prefixSchedule = root.get_prefix_schedule_union_map();
  isl::union_map accessesLeafCoreStmtR = applySchedule(prefixSchedule, reads);
  isl::union_map accessesLeafCoreStmtW = applySchedule(prefixSchedule, writes);
  accessesLeafCoreStmtR = accessesLeafCoreStmtR.subtract(accessesLeafInitStmtR); 
  accessesLeafCoreStmtW = accessesLeafCoreStmtW.subtract(accessesLeafInitStmtW);
 
  using namespace matchers; 
  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto _ii = placeholder(ctx);
  auto _jj = placeholder(ctx);

  auto _A = arrayPlaceholder();
  auto _B = arrayPlaceholder();
  auto _C = arrayPlaceholder();

  auto psRead =
      allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
  auto readMatches = match(accessesLeafCoreStmtR, psRead);
  ASSERT_EQ(readMatches.size(), 1u);
  auto psWrite = allOf(access(_A, _ii, _jj));
  auto writeMatches = match(accessesLeafCoreStmtW, psWrite);
  ASSERT_EQ(writeMatches.size(), 1u);

  // check index for read and write are equal
  ASSERT_TRUE(writeMatches[0][_ii].payload().inputDimPos_ ==
              readMatches[0][_i].payload().inputDimPos_);
  ASSERT_TRUE(writeMatches[0][_jj].payload().inputDimPos_ ==
              readMatches[0][_j].payload().inputDimPos_);


  std::vector<isl::space> v_space = writeMatches[0][_ii].candidateSpaces();
  ASSERT_TRUE(v_space.size() == 1);

  // layout info for matmul.
  MatMulInfo MMI;

  MMI.WriteToC = 
    v_space[0].range().unwrap().range().get_tuple_name(isl::dim::out);


  MMI.i = writeMatches[0][_ii].payload().inputDimPos_;
  MMI.j = writeMatches[0][_jj].payload().inputDimPos_;
  MMI.k = readMatches[0][_k].payload().inputDimPos_;
 
  fillMMI(arrayInfo, MMI, prefixSchedule);    
 
  ASSERT_TRUE(MMI.A.compare("A") == 0); 
  ASSERT_TRUE(MMI.B.compare("B") == 0);
  ASSERT_TRUE(MMI.ReadFromC.compare("C") == 0);
  ASSERT_TRUE(MMI.WriteToC.compare("C") == 0);
  
  // go to matmul node.
  isl::schedule_node node = root.root().child(0);
  isl::union_set domain = node.get_domain();
  bool singleStatement = (domain.n_set() == 1);
  ASSERT_EQ(singleStatement, false);

  node = addCuBLASHandleStartUp(node);
  node = addCuBLASHandleTearDown(node);
  node = addCopyToDevice(node);
  node = addCopyFromDevice(node);
  node = addKernelBoundaries(node);

  petScop.schedule() = node.get_schedule();
  static_cast<isl::schedule>(petScop.schedule()).dump();

  std::vector<MatMulInfo> MMIBatched;
  MMIBatched.push_back(MMI);
  payloadCodegen p = {arrayInfo, MMIBatched};

  //std::string codeGen = petScop.codegenPayload(codeGenGPU, &p);
  //codeGen = insertCallToCUBALS(codeGen, &p);
  //std::cout << codeGen << std::endl;
}

TEST(Transformers, checkPetArrayExtraction) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop =
    pet::Scop::parseFile(ctx, "inputs/one-dimensional-init.c").getScop();
  ASSERT_EQ(scop.n_array, 1u);

  for(int i = 0; i < scop.n_array; ++i) {
    ASSERT_EQ(scop.arrays[i].element_type, "float");
  }
}

//std::pair<bool, isl::schedule_node> 
//  getTopmostBand(const matchers::ScheduleNodeMatcher &m, isl::schedule_node root) {
//
//  assert(root.get() && "invalid node");
//  std::pair <bool, isl::schedule_node> res;
//
//  std::stack<isl::schedule_node> nodeStack;
//  nodeStack.push(root);
//
//  while(nodeStack.empty() == false) {
//    isl::schedule_node node = nodeStack.top();
//    nodeStack.pop();
//    
//    if(matchers::ScheduleNodeMatcher::isMatching(m, node)) {
//      res = std::make_pair(true, node);
//      return res;
//    }
//
//    size_t n_children =
//      static_cast<size_t>(isl_schedule_node_n_children(node.get()));
//    for(size_t i=0; i<n_children; ++i) {
//      nodeStack.push(node.child(i));
//    }
//  }
//  res = std::make_pair(false, nullptr);
//  return res;
//}
 

//TEST(Transformers, countBandNodes) {
//  auto ctx = ScopedCtx(pet::allocCtx());
//  auto scop =
//    pet::Scop::parseFile(ctx, "inputs/nested.c").getScop();
//  isl::schedule_node root = scop.schedule.get_root();
//  
//  isl::schedule_node parent, child;
//  auto matcher = [&]() {
//    using namespace matchers;
//    return band(parent, anyTree(child));
//  }();
//
//
//  int counter = 0;
//  std::pair<bool, isl::schedule_node> res;
//  do {
//    res = getTopmostBand(matcher, root);
//    if(res.second.get()) {
//      counter++;
//      root = res.second.child(0);
//    }
//  } while(res.first);
//
//  ASSERT_TRUE(counter == 4);
//}

//TEST(Transformers, locateTopMostBand) {
//  auto ctx = ScopedCtx(pet::allocCtx());
//  auto scop =
//    pet::Scop::parseFile(ctx, "inputs/nested.c").getScop();
//  isl::schedule_node root = scop.schedule.get_root();
//  
//  isl::schedule_node parent, child;
//  auto matcher = [&]() {
//    using namespace matchers;
//    return band(parent, anyTree(child));
//  }();
//
//  auto res = getTopmostBand(matcher, root);
//
//  ASSERT_TRUE(res.second.get());
//}
  

//TEST(Transformers, ExtractMultipleScop) {
//  auto ctx = ScopedCtx(pet::allocCtx());
//  std::string in = "inputs/doubleScop.c";
//  ScopContainer c;
//  c = pet::Scop::parseMultipleScop(ctx, in);  
//  ASSERT_TRUE(c.c.size() == 2);
//
//  auto pet_scop = pet::Scop(c.c[0]);
//  std::string transpose = "C[c0][c1] = D[c1][c0];";
//  std::string result = pet_scop.codegen();
//  auto stmt = result.find(transpose);
//  ASSERT_TRUE(stmt != std::string::npos);
//
//  auto pet_scop_following = pet::Scop(c.c[1]);
//  std::string stencil = 
//    "B[c1] = (0.33333 * ((A[c1 - 1] + A[c1]) + A[c1 + 1]));";
// result = pet_scop_following.codegen();
//  stmt = result.find(stencil);
//  ASSERT_TRUE(stmt != std::string::npos);
//}
  
TEST(Transformer, Capture) {
  isl::schedule_node bandNode, filterNode1, filterNode2, filterSubtree;
  auto ctx = isl::ctx(isl_ctx_alloc());

  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return
      band(bandNode,
        sequence(
          filter(filterNode1,
            leaf()),
          filter(filterNode2,
            anyTree(filterSubtree))));
    // clang-format on
  }();

  auto node = [ctx]() {
    using namespace builders;
    auto iterationDomain = isl::union_set(
        ctx, "{S1[i,j]: 0 <= i,j < 10; S2[i,j,k]: 0 <= i,j,k < 42}");
    auto sched =
        isl::multi_union_pw_aff(ctx, "[{S1[i,j]->[(i)]; S2[i,j]->[(i)]}, "
                                     "{S1[i,j]->[(j)]; S2[i,j]->[(j)]}]");
    auto filterS1 = isl::union_set(ctx, "{S1[i,j]}");
    auto filterS2 = isl::union_set(ctx, "{S2[i,j]}");
    auto innerSched = isl::multi_union_pw_aff(ctx, "[{S2[i,j,k]->[(k)]}]");

    // clang-format off
    auto builder =
      domain(iterationDomain,
        band(sched,
          sequence(
            filter(filterS1),
            filter(filterS2,
              band(innerSched)))));
    // clang-format on

    return builder.build();
  }();

  // Let's find a node.
  // We don't have matcher-based lookups, so lets just use node.child(0) for
  // now.
  ASSERT_TRUE(
      matchers::ScheduleNodeMatcher::isMatching(matcher, node.child(0)));
  node.dump();

  // Let's transform!
  auto transformedBuilder = [&]() {
    auto filter1 = filterNode1.filter_get_filter();
    auto filter2 = filterNode2.filter_get_filter();
    auto schedule = bandNode.band_get_partial_schedule();

    using namespace builders;
    // clang-format off
    return
      sequence(
        filter(filter1,
          band(schedule.intersect_domain(filter1))),
        filter(filter2,
          band(schedule.intersect_domain(filter2),
            subtree(filterSubtree))));
    // clang-format on
  }();
  node = node.child(0);
  node = node.cut();
  node = transformedBuilder.insertAt(node);
  node = node.parent();

  node.dump();
}

TEST(schedule, MergeBandsCallLambda) {

  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop = pet::Scop::parseFile(
    ScopedCtx(pet::allocCtx()), "inputs/nested.c").getScop();
/*
  isl::schedule_node parent, child, grandchild;
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(child,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Capturing the matched nodes by-reference since they don't have any values
  // until the matcher was called on a tree.
  // Note that we don't call the lambda yet.
  auto merger = [&]() {
    using namespace builders;
    auto schedule = parent.band_get_partial_schedule().flat_range_product(
        child.band_get_partial_schedule());
    // clang-format off
    return band(schedule,
             subtree(grandchild));
    // clang-format on
  };

  // Keep transforming the tree while possible.
  // Call the builder lambda each time to construct a new builder based on the
  // currently matched nodes (captured by-reference above).
  auto node = scop.schedule.get_root().child(0);
  while (matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
    node = node.cut();
    node = merger().insertAt(node);
  }
  using namespace matchers;
  EXPECT_TRUE(matchers::ScheduleNodeMatcher::isMatching(band(leaf()), node));
*/  
}

/*
TEST_F(Schedule, MergeBandsDeclarative) {
  isl::schedule_node parent, child, grandchild;
  // Note that the lambda is called immediately and is only necessary for
  // compound initialization (matchers are not copyable).
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(child,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Use lambdas to lazily initialize the builder with the nodes and values yet
  // to be captured by the matcher.
  auto declarativeMerger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule = [&]() {
      return parent.band_get_partial_schedule().flat_range_product(
          child.band_get_partial_schedule());
    };
    auto st = [&]() { return subtreeBuilder(grandchild); };
    declarativeMerger = band(schedule, subtree(st));
  }

  // Keep transforming the tree while possible.
  auto node = topmostBand();
  while (matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
    node = node.cut();
    node = declarativeMerger.insertAt(node);
  }

  expectSingleBand(node);
}
*/

static isl::union_map computeAllDependences(const Scop &scop) {
  // For the simplest possible dependence analysis, get rid of reference tags.
  auto reads = scop.reads.domain_factor_domain();
  auto mayWrites = scop.mayWrites.domain_factor_domain();
  auto mustWrites = scop.mustWrites.domain_factor_domain();

  // False dependences (output and anti).
  // Sinks are writes, sources are reads and writes.
  auto falseDepsFlow = isl::union_access_info(mayWrites.unite(mustWrites))
                           .set_may_source(mayWrites.unite(reads))
                           .set_must_source(mustWrites)
                           .set_schedule(scop.schedule)
                           .compute_flow();

  isl::union_map falseDeps = falseDepsFlow.get_may_dependence();

  // Flow dependences.
  // Sinks are reads and sources are writes.
  auto flowDepsFlow = isl::union_access_info(reads)
                          .set_may_source(mayWrites)
                          .set_must_source(mustWrites)
                          .set_schedule(scop.schedule)
                          .compute_flow();

  isl::union_map flowDeps = flowDepsFlow.get_may_dependence();

  return flowDeps.unite(falseDeps);
}

// The partial schedule is only defined for those domain elements that passed
// through filters until "node".  Therefore, there is no need to explicitly
// introduce auxiliary dimensions for the filters.
static inline isl::union_map
filterOutCarriedDependences(isl::union_map dependences,
                            isl::schedule_node node) {
  auto partialSchedule = node.get_prefix_schedule_multi_union_pw_aff();
  return dependences.eq_at(partialSchedule);
}

static bool canMerge(isl::schedule_node parentBand,
                     isl::union_map dependences) {
  // Permutability condition: there are no negative distances along the
  // dimensions that are not carried until now by any of dimensions.
  auto t1 = parentBand.band_get_partial_schedule();
  auto t2 = parentBand.child(0).band_get_partial_schedule();
  auto schedule = isl::union_map::from(t1.flat_range_product(t2));
  auto scheduleSpace = isl::set(schedule.range()).get_space();
  auto positiveOrthant =
      isl::set(isl::basic_set::positive_orthant(scheduleSpace));
  dependences = filterOutCarriedDependences(dependences, parentBand);
  return dependences.apply_domain(schedule)
      .apply_range(schedule)
      .deltas()
      .is_subset(positiveOrthant);
}

static inline isl::schedule_node
rebuild(isl::schedule_node node,
        const builders::ScheduleNodeBuilder &replacement) {
  // this may not be always legal...
  node = node.cut();
  node = replacement.insertAt(node);
  return node;
}

isl::schedule_node
replaceOnce(isl::schedule_node node,
            const matchers::ScheduleNodeMatcher &pattern,
            const builders::ScheduleNodeBuilder &replacement) {
  if (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
replaceRepeatedly(isl::schedule_node node,
                  const matchers::ScheduleNodeMatcher &pattern,
                  const builders::ScheduleNodeBuilder &replacement) {
  while (matchers::ScheduleNodeMatcher::isMatching(pattern, node)) {
    node = rebuild(node, replacement);
  }
  return node;
}

isl::schedule_node
replaceDFSPreorderRepeatedly(isl::schedule_node node,
                             const matchers::ScheduleNodeMatcher &pattern,
                             const builders::ScheduleNodeBuilder &replacement) {
  node = replaceRepeatedly(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderRepeatedly(node.child(i), pattern, replacement)
               .parent();
  }
  return node;
}

isl::schedule_node
replaceDFSPreorderOnce(isl::schedule_node node,
                       const matchers::ScheduleNodeMatcher &pattern,
                       const builders::ScheduleNodeBuilder &replacement) {
  node = replaceOnce(node, pattern, replacement);
  for (int i = 0; i < node.n_children(); ++i) {
    node = replaceDFSPreorderOnce(node.child(i), pattern, replacement).parent();
  }
  return node;
}

isl::schedule_node mergeIfTilable(isl::schedule_node node,
                                  isl::union_map dependences) {
  isl::schedule_node parent, child, grandchild;

  auto canMergeCaptureChild = [&child, dependences](isl::schedule_node node) {
    if (canMerge(node.parent(), dependences)) {
      child = node;
      return true;
    }
    return false;
  };

  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(canMergeCaptureChild,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Use lambdas to lazily initialize the builder with the nodes and values yet
  // to be captured by the matcher.
  auto declarativeMerger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule = [&]() {
      auto descr =
          BandDescriptor(parent.band_get_partial_schedule().flat_range_product(
              child.band_get_partial_schedule()));
      descr.permutable = 1;
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(grandchild); };
    declarativeMerger = band(schedule, subtree(st));
  }

  return replaceDFSPreorderRepeatedly(node, matcher, declarativeMerger);
}
/*
TEST_F(Schedule, MergeBandsIfTilable) {
  auto dependences = computeAllDependences(scop_);
  auto node = mergeIfTilable(topmostBand(), dependences);
  expectSingleBand(node);
  EXPECT_EQ(isl_schedule_node_band_get_permutable(node.get()), isl_bool_true);
}
*/
static std::vector<bool> detectCoincidence(isl::schedule_node band,
                                           isl::union_map dependences) {
  std::vector<bool> result;
  auto schedule = band.band_get_partial_schedule();
  int dim = schedule.dim(isl::dim::set);
  if (dependences.is_empty()) {
    result.resize(dim, true);
    return result;
  }
  for (int i = 0; i < dim; ++i) {
    auto upa = schedule.get_union_pw_aff(i);
    auto partialSchedule = isl::union_map::from_union_pw_aff(upa);
    auto deltas = isl::set(dependences.apply_domain(partialSchedule)
                               .apply_range(partialSchedule)
                               .deltas());
    auto zeroSet = [&]() {
      auto lspace = isl::local_space(deltas.get_space());
      auto aff = isl::aff::var_on_domain(lspace, isl::dim::set, 0);
      auto zeroAff = isl::aff(lspace);
      using set_maker::operator==;
      return isl::set(aff == zeroAff);
    }();
    result.push_back(deltas.is_subset(zeroSet));
  }
  return result;
}

isl::schedule_node markCoincident(isl::schedule_node root,
                                  isl::union_map dependences) {
  isl::schedule_node node, child;
  auto matcher = [&]() {
    using namespace matchers;
    return band(node, anyTree(child));
  }();

  auto marker = [&]() {
    auto descr = builders::BandDescriptor(node.band_get_partial_schedule());
    descr.coincident = detectCoincidence(node, dependences);
    return descr;
  };

  auto builder = [&]() {
    using namespace builders;
    return band(marker, subtree(child));
  }();

  return replaceDFSPreorderOnce(root, matcher, builder);
}
/*
TEST_F(Schedule, MarkCoincident) {
  auto dependences = computeAllDependences(scop_);
  markCoincident(scop_.schedule.get_root(), dependences).dump();
}
*/
static bool canSink(isl::schedule_node band) {
  auto dim = band.band_get_partial_schedule().dim(isl::dim::set);
  if (dim < 2) {
    return false;
  }

  auto permutable =
      isl_schedule_node_band_get_permutable(band.get()) == isl_bool_true;
  if (!permutable) {
    return false;
  }

  return true;
}

// pluto-style sinking
// assuming access relations with tags in the range
static int findSinkable(isl::union_map accesses, isl::schedule_node band) {
  auto schedule = band.band_get_partial_schedule();
  auto nDim = schedule.dim(isl::dim::set);
  auto ctx = accesses.get_ctx();

  std::vector<int64_t> weights;
  weights.reserve(nDim);
  for (unsigned i = 0; i < nDim; ++i) {

    auto schedule1D = schedule.get_union_pw_aff(i);
    auto scheduleMap1D = isl::union_map::from_union_pw_aff(schedule1D);
    auto scheduledAccess = accesses.apply_domain(scheduleMap1D);

    using namespace matchers;
    int nRepeated =
        match(scheduledAccess, allOf(access(dim(-1, stride(ctx, 0))))).size();
    int nLocal = 0;
    for (int s = 1; s <= 4; ++s) {
      nLocal +=
          match(scheduledAccess, allOf(access(dim(-1, stride(ctx, s))))).size();
    }
    int nAccesses = scheduledAccess.n_map();
    int nNonLocal = nAccesses - nRepeated - nLocal;
    bool isVectorizable = nNonLocal == 0;

    // count # of stride-zero (+4 per access)
    // count # of stride-one (+2 per access)
    // is vectorizable <= # of stride-zero + # of stride-one = # of accesses
    // (bonus 8) all other strides (-16 per access)
    weights.push_back(2 * nLocal + 4 * nRepeated + 8 * isVectorizable -
                      16 * nNonLocal);
  }

  auto maxWeightIter = std::max_element(weights.begin(), weights.end());
  return std::distance(weights.begin(), maxWeightIter);
}

TEST(Transformer, SinkLocal) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto scop = pet::Scop::parseFile(ctx, "inputs/1mm_fused.c").getScop();

  auto dependences = computeAllDependences(scop);
  scop.schedule =
      mergeIfTilable(scop.schedule.get_root(), dependences).get_schedule();

  isl::schedule_node node, child;
  auto matcher = matchers::band(
      [&node](isl::schedule_node n) {
        if (canSink(n)) {
          node = n;
          return true;
        }
        return false;
      },
      matchers::anyTree(child));

  isl::union_map accesses =
      scop.reads.unite(scop.mayWrites).unite(scop.mustWrites).curry();

  builders::ScheduleNodeBuilder builder = builders::band(
      [&node, &accesses]() {
        int pos = findSinkable(accesses, node);
        auto schedule = node.band_get_partial_schedule();
        auto scheduleAtPos = schedule.get_union_pw_aff(pos);
        schedule = schedule.drop_dims(isl::dim::set, pos, 1);
        schedule =
            schedule.flat_range_product(isl::multi_union_pw_aff(scheduleAtPos));

        builders::BandDescriptor descriptor(node);
        descriptor.partialSchedule = schedule;
        auto isCoincident = descriptor.coincident.at(pos);
        descriptor.coincident.erase(descriptor.coincident.begin() + pos);
        descriptor.coincident.push_back(isCoincident);
        return descriptor;
      },
      builders::subtree(child));

  node = replaceDFSPreorderOnce(scop.schedule.get_root(), matcher, builder);

  // Check that we indeed sink the "j" loop.
  // clang-format off
  auto expected = isl::union_map(ctx,
      "{ S_0[i, j, k] -> [o0, o1, o2, o3] : o0 = i and o1 = k and o2 = j and o3 = 0;"
        "S_1[i, j, k] -> [o0, o1, o2, o3] : o0 = i and o1 = k and o2 = j and o3 = 1 }");
  // clang-format on
  EXPECT_TRUE(node.get_schedule().get_map().is_subset(expected));
}

// Check that all relevant parts of the code (loops and transformed statements)
// are correctly generated.  In particular, check that loops are generated in
// the right order.  Whitespace is ignored.
TEST(Transformer, Codegen) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/nested.c");

  std::string loop1 = "for (int c0 = 0; c0 <= min(1023, n - 2); c0 += 1)";
  std::string loop2 = "for (int c1 = 0; c1 < n - c0 - 1; c1 += 1)";
  std::string loop3 = "for (int c2 = n - 1; c2 <= n + 41; c2 += 1)";
  std::string loop4 = "for (int c3 = c0 + 1; c3 < n - c1; c3 += 1)";
  std::string stmt = "foo((c0), (c1), (c2), (c3));";
  auto result = petScop.codegen();

  auto loop1pos = result.find(loop1);
  auto loop2pos = result.find(loop2, loop1pos + loop1.length());
  auto loop3pos = result.find(loop3, loop2pos + loop2.length());
  auto loop4pos = result.find(loop4, loop3pos + loop3.length());
  auto stmtpos = result.find(stmt, loop4pos + loop4.length());

  // Note that we don't care about the particular positions in the string, only
  // that the relation between them holds. Therefore we use ASSERT_TRUE on
  // relations to avoid useless and potentially large (npos) numbers output in
  // case an assertion fails.
  ASSERT_TRUE(loop1pos != std::string::npos);
  ASSERT_TRUE(loop2pos != std::string::npos);
  ASSERT_TRUE(loop3pos != std::string::npos);
  ASSERT_TRUE(loop4pos != std::string::npos);
  ASSERT_TRUE(stmtpos != std::string::npos);

  ASSERT_TRUE(loop2pos > loop1pos);
  ASSERT_TRUE(loop3pos > loop2pos);
  ASSERT_TRUE(loop4pos > loop3pos);
  ASSERT_TRUE(stmtpos > loop4pos);
}

TEST(Transformer, InjectStatement) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/stencil.c");

  isl::schedule_node node;
  auto matcher = [&]() {
    using namespace matchers;
    return anyTree(node);
  }();

  matchers::ScheduleNodeMatcher::isMatching(
      matcher, petScop.getScop().schedule.get_root().child(0));

  auto builder = [&]() {
    using namespace builders;
    return extension(
        isl::union_map(ctx, "[] -> {[]->someLongAndHopefullyUniqueName[]:}"),
        sequence(filter(isl::union_set(
                     ctx, "[] -> {someLongAndHopefullyUniqueName[]:}")),
                 filter(petScop.getScop().domain().universe(), subtree(node))));
  }();

  auto sched = builder.insertAt(petScop.getScop().schedule.get_root().child(0))
                   .get_schedule();
  petScop.schedule() = sched;
  auto code = petScop.codegen();
  EXPECT_TRUE(code.find("someLongAndHopefullyUniqueName") != std::string::npos);
}

static isl::multi_union_pw_aff getSchedulePointTile(isl::schedule_node node,
                                                    isl::multi_union_pw_aff t) {
  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  return sched.sub(t);
}

static isl::multi_union_pw_aff getScheduleTile(isl::schedule_node node,
                                               std::vector<int> tileSizes) {
  assert(tileSizes.size() != 0 && "empty tileSizes array");
  isl::space space = isl::manage(isl_schedule_node_band_get_space(node.get()));
  unsigned dims = space.dim(isl::dim::set);
  assert(dims == tileSizes.size() &&
         "number of dimensions should match tileSizes size");

  isl::multi_val sizes = isl::multi_val::zero(space);
  for (unsigned i = 0; i < dims; ++i) {
    int tileSize = tileSizes[i];
    sizes = sizes.set_val(i, isl::val(node.get_ctx(), tileSize));
  }

  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  for (unsigned i = 0; i < dims; ++i) {

    isl::union_pw_aff upa = sched.get_union_pw_aff(i);
    isl::val v = sizes.get_val(i);
    upa = upa.scale_down_val(v);
    upa = upa.floor();
    sched = sched.set_union_pw_aff(i, upa);
  }
  return sched;
}

/*
static isl::multi_union_pw_aff swapDims(isl::multi_union_pw_aff ps,
                                        int firstDim, int secondDim) {
  auto scheduleFirstDim = ps.get_union_pw_aff(firstDim);
  auto scheduleSecondDim = ps.get_union_pw_aff(secondDim);
  ps = ps.set_union_pw_aff(secondDim, scheduleFirstDim);
  ps = ps.set_union_pw_aff(firstDim, scheduleSecondDim);
  return ps;
}
*/

TEST(Transformer, synthesis) {

  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/1mmWithoutInitStmt.c");
  auto scop = petScop.getScop();

  auto dependences = computeAllDependences(scop);
  scop.schedule =
    mergeIfTilable(scop.schedule.get_root(), dependences).get_schedule();

  isl::union_map reads = scop.reads.curry();
  isl::union_map writes = scop.mustWrites.curry();

  using namespace matchers;

  typedef Placeholder<SingleInputDim,UnfixedOutDimPattern<SimpleAff>> Placeholder;
  struct PlaceholderSet {
    Placeholder p;
    std::string id;
  };
  struct ArrayPlaceholderSet {
    ArrayPlaceholder ap;
    std::string id;
  };

  std::vector<PlaceholderSet> vectorPlaceholderSet = {};
  std::vector<ArrayPlaceholderSet> vectorArrayPlaceholderSet = {};
  
  std::string id = "_i";
  for (size_t i = 0; i < 3; i++) {
    PlaceholderSet tmp = {placeholder(ctx), id+std::to_string(i)}; 
    vectorPlaceholderSet.push_back(tmp);
  }

  id = "_C";
  for (size_t i = 0; i < 3; i++) {
    ArrayPlaceholderSet tmp = {arrayPlaceholder(), id+std::to_string(i)};
    vectorArrayPlaceholderSet.push_back(tmp);
  }

  //for (size_t i = 0; i < 3; i++) {
  //  std::cout << vectorPlaceholderSet[i].id << std::endl;
  //  std::cout << vectorArrayPlaceholderSet[i].id << std::endl;
  //}

  typedef ArrayPlaceholderList<SingleInputDim, FixedOutDimPattern<SimpleAff>> Access;
  std::vector<Access> accessList = {};

  accessList.push_back(access(vectorArrayPlaceholderSet[0].ap,  // _C0 
                              vectorPlaceholderSet[0].p,        // _i0 -> i
                              vectorPlaceholderSet[1].p));      // _i1 -> j
  accessList.push_back(access(vectorArrayPlaceholderSet[1].ap,
                              vectorPlaceholderSet[0].p,
                              vectorPlaceholderSet[2].p));
  accessList.push_back(access(vectorArrayPlaceholderSet[2].ap,
                              vectorPlaceholderSet[2].p,
                              vectorPlaceholderSet[1].p));
  auto psRead = allOf(accessList);
  auto readMatches = match(reads, psRead);

  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto _C = arrayPlaceholder();
  auto _B = arrayPlaceholder();
  auto _A = arrayPlaceholder();

  auto psReadOrig = 
    allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
  auto readMatchesOrig = match(reads, psReadOrig);
  
  EXPECT_EQ(readMatches[0][vectorPlaceholderSet[0].p].payload().inputDimPos_,
            readMatchesOrig[0][_i].payload().inputDimPos_);
  EXPECT_EQ(readMatches[0][vectorPlaceholderSet[1].p].payload().inputDimPos_,
            readMatchesOrig[0][_j].payload().inputDimPos_);
  EXPECT_EQ(readMatches[0][vectorPlaceholderSet[2].p].payload().inputDimPos_,
            readMatchesOrig[0][_k].payload().inputDimPos_);
  
}

TEST(Transformer, MatchMatmul) {

  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/1mmWithoutInitStmt.c");
  auto scop = petScop.getScop();

  auto dependences = computeAllDependences(scop);
  scop.schedule =
      mergeIfTilable(scop.schedule.get_root(), dependences).get_schedule();

  isl::schedule_node root = scop.schedule.get_root();

  using namespace matchers;
  isl::schedule_node node;
  // clang-format off
  auto matcher = band(
    [&node] (isl::schedule_node n) {
      if (isl_schedule_node_band_n_member(n.get()) < 3) {
        return false;
      } else {
        node = n;
        return true;
      }
    },
    leaf());
  // clang-format on

  ASSERT_TRUE(ScheduleNodeMatcher::isMatching(matcher, root.child(0)));
/*
  isl::union_map reads = scop.reads.curry();
  isl::union_map writes = scop.mustWrites.curry();

  auto _i = placeholder(ctx);
  auto _j = placeholder(ctx);
  auto _k = placeholder(ctx);
  auto _ii = placeholder(ctx);
  auto _jj = placeholder(ctx);

  auto _A = arrayPlaceholder();
  auto _B = arrayPlaceholder();
  auto _C = arrayPlaceholder();

  auto psRead =
      allOf(access(_A, _i, _j), access(_B, _i, _k), access(_C, _k, _j));
  auto readMatches = match(reads, psRead);
  auto psWrite = allOf(access(_A, _ii, _jj));
  auto writeMatches = match(writes, psWrite);

  // tmp[j][i] = alpha * A[i][k] * B[k][j] + tmp[i][j]
  // pass the checks. this is because we do not link
  // read and write at the moment. Placeholder are _not_
  // reused between different calls to allOf. We can overcome
  // this inspecting the placeholder for the write and the read.
  // They should be equal.

  ASSERT_EQ(readMatches.size(), 1u);
  ASSERT_EQ(writeMatches.size(), 1u);

  // check index for read and write are equal
  ASSERT_TRUE(writeMatches[0][_ii].payload().inputDimPos_ ==
              readMatches[0][_i].payload().inputDimPos_);
  ASSERT_TRUE(writeMatches[0][_jj].payload().inputDimPos_ ==
              readMatches[0][_j].payload().inputDimPos_);

  // D[i][j] = alpha * A[i][k] * B[k][j] + tmp[i][j]
  // pass the test. We may want to apply the same check
  // as before also for the accessed array.

  // step 1. Loop interchange.
  // Interchange the loops in the loop nest such that
  // j is the outermost loop followed by k and i.
  int iPosOriginal = readMatches[0][_i].payload().inputDimPos_;
  int jPosOriginal = readMatches[0][_j].payload().inputDimPos_;
  int kPosOriginal = readMatches[0][_k].payload().inputDimPos_;

  // transformer to interchange dimensions
  using namespace builders;
  ScheduleNodeBuilder swapDimensions =
      band([&node, &iPosOriginal, &jPosOriginal, &kPosOriginal]() {
        auto originalSchedule = node.band_get_partial_schedule();
        auto newSchedule = originalSchedule;
        if (jPosOriginal != 0) {
          if (iPosOriginal == 0) {
            newSchedule = swapDims(newSchedule, jPosOriginal, iPosOriginal);
            iPosOriginal = jPosOriginal;
            jPosOriginal = 0;
          }
          if (kPosOriginal == 0) {
            newSchedule = swapDims(newSchedule, jPosOriginal, kPosOriginal);
            kPosOriginal = jPosOriginal;
            jPosOriginal = 0;
          }
        }
        if (kPosOriginal != 1) {
          newSchedule = swapDims(newSchedule, kPosOriginal, iPosOriginal);
        }
        return newSchedule;
      });

  node = rebuild(node, swapDimensions);
  root = node.root();
  petScop.schedule() = root.get_schedule();
  std::string loopJ = "for (int c0 = 0; c0 <= 1023; c0 += 1)";
  std::string loopK = "for (int c1 = 0; c1 <= 1023; c1 += 1)";
  std::string loopI = "for (int c2 = 0; c2 <= 1023; c2 += 1)";
  std::string stmt =
      "tmp[c2][c0] = ((((alpha) * A[c2][c1]) * B[c1][c0]) + tmp[c2][c0]);";
  auto result = petScop.codegen();
  auto loopJPos = result.find(loopJ);
  auto loopKPos = result.find(loopK, loopJPos + loopJ.length());
  auto loopIPos = result.find(loopI, loopKPos + loopK.length());
  auto stmtPos = result.find(stmt, loopIPos + loopI.length());
  ASSERT_TRUE(loopJPos != std::string::npos);
  ASSERT_TRUE(loopKPos != std::string::npos);
  ASSERT_TRUE(loopIPos != std::string::npos);
  ASSERT_TRUE(stmtPos != std::string::npos);
  ASSERT_TRUE(loopKPos > loopJPos);
  ASSERT_TRUE(loopIPos > loopKPos);
  ASSERT_TRUE(stmtPos > loopIPos);

  // step 2. create macro-kernel
  // For the micro and macro kernels we assume
  // given values for the tile size.
  // Note: In polly the interchange is performed
  // on the tile loops, while in the paper on the
  // point loops we follow the paper.
  // We tile all the three loops j p and i
  // to create jc pc and ic and we interchange
  // pc and ic. We use the same tile factor of 32
  // for all the dimensions.
  using namespace builders;
  // set tile values manually
  int dimOutNum = isl_schedule_node_band_n_member(node.get());
  std::vector<int> tileSizes(dimOutNum);
  tileSizes = {32, 32, 32};

  // tile node and get partial schedule
  auto tileSchedule = getScheduleTile(node, tileSizes);
  auto pointSchedule = getSchedulePointTile(node, tileSchedule);

  // clang-format off
  ScheduleNodeBuilder macroKernel =
    band(tileSchedule,
      band(
        [&pointSchedule, &dimOutNum]() {
          auto newPartialSchedule =
              swapDims(pointSchedule, dimOutNum - 2, dimOutNum - 1);
          return newPartialSchedule;
        }));
  // clang-format on

  node = rebuild(node, macroKernel);
  root = node.root();
  petScop.schedule() = root.get_schedule();
  loopJ = "for (int c0 = 0; c0 <= 31; c0 += 1)";
  loopK = "for (int c1 = 0; c1 <= 31; c1 += 1)";
  loopI = "for (int c2 = 0; c2 <= 31; c2 += 1)";
  std::string loopJc = "for (int c3 = 31 * c0; c3 <= 31 * c0 + 31; c3 += 1)";
  std::string loopIc = "for (int c4 = 31 * c2; c4 <= 31 * c2 + 31; c4 += 1)";
  std::string loopKc = "for (int c5 = 31 * c1; c5 <= 31 * c1 + 31; c5 += 1)";
  stmt = "tmp[c2 + c4][c0 + c3] = ((((alpha) * A[c2 + c4][c1 + c5]) * B[c1 + "
         "c5][c0 + c3]) + tmp[c2 + c4][c0 + c3]);";
  result = petScop.codegen();
  auto loopJcPos = result.find(loopJc);
  auto loopIcPos = result.find(loopIc);
  auto loopKcPos = result.find(loopKc);
  ASSERT_TRUE(loopJcPos != std::string::npos);
  ASSERT_TRUE(loopKcPos != std::string::npos);
  ASSERT_TRUE(loopIcPos != std::string::npos);
  ASSERT_TRUE(loopKcPos > loopIcPos);
  ASSERT_TRUE(loopIcPos > loopJcPos);

  // match micro-kernel
  auto matcherMicroKernel = [&]() {
    using namespace matchers;
    return band(node, leaf());
  }();

  ASSERT_TRUE(ScheduleNodeMatcher::isMatching(matcherMicroKernel,
                                              root.child(0).child(0)));

  // create micro-kernel
  // tile ic and jc with a tile factor of 2
  // do not tile pc. The tiling produces two new
  // loops ir and jr.
  tileSizes = {2, 2, 1};
  // tile node and get partial schedule
  tileSchedule = getScheduleTile(node, tileSizes);
  pointSchedule = getSchedulePointTile(node, tileSchedule);

  // clang-format off
  ScheduleNodeBuilder microKernel =
      band(tileSchedule,
        band([&node, &pointSchedule]() {
          auto descr = BandDescriptor(pointSchedule);
          descr.astOptions =
            isl::union_set(node.get_ctx(), "{unroll[x]}");
          return descr;
        }));
  // clang-format on

  node = rebuild(node, microKernel);
  root = node.root();
  petScop.schedule() = root.get_schedule();
  result = petScop.codegen();
  std::cout << result << std::endl;
*/
}

isl::union_map addRangeId(isl::union_map umap, const std::string &tag) {
  auto id =
      isl::manage(isl_id_alloc(umap.get_ctx().get(), tag.c_str(), nullptr));
  // TODO: make this possible with matchers as well
  auto result = isl::union_map::empty(umap.get_space());
  umap.foreach_map([id, &result](isl::map m) {
    result = result.add_map(m.set_tuple_id(isl::dim::out, id));
    return isl_stat_ok;
  });
  return result;
}

// expecting scheduled access
isl::map make1dDLT(isl::map access, int size) {
  using namespace aff_op;
  access = access.coalesce();
  auto allPoints =
      isl::map::from_domain_and_range(access.range(), access.range());
  isl::pw_aff min = allPoints.dim_min(0);
  isl::pw_aff dist = allPoints.dim_max(0) - min + 1;
  // TODO: cut off: o0 > size * (dist / size)

  auto dlt = isl::map::empty(access.range().get_space().map_from_set());
  auto a = isl::aff::var_on_domain(isl::local_space(access.range().get_space()),
                                   isl::dim::set, 0);
  auto _i = isl::pw_aff(a);
  for (long s = 0; s < size; ++s) {
    auto lhs = s + size * (_i - min - s * dist / size);
    using namespace map_maker;
    dlt = dlt.unite((lhs == _i)
                        .intersect(_i >= min + s * dist / size)
                        .intersect(_i < min + (s + 1) * dist / size));
  }
  std::string arrayName = dlt.range().get_tuple_id().get_name();
  isl::id dltArrayId =
      isl::id::alloc(dlt.get_ctx(), "_dlt_" + arrayName, nullptr);
  return dlt.set_tuple_id(isl::dim::out, dltArrayId);
}

static __isl_give isl_multi_pw_aff *
transformSubscriptsDLT(__isl_take isl_multi_pw_aff *subscript,
                       __isl_keep isl_id *, void *user) {
  auto access = isl::manage(subscript);
  auto iteratorMap = isl::manage_copy(static_cast<isl_pw_multi_aff *>(user));
  auto scheduledAccess = access.pullback(iteratorMap);

  int dim = scheduledAccess.dim(isl::dim::set);
  for (int i = 0; i < dim; ++i) {
    auto pa = scheduledAccess.get_pw_aff(i);
    auto result = isl::pw_aff(isl::set::empty(pa.domain().get_space()),
                              isl::val::zero(pa.get_ctx()));
    pa.foreach_piece([&result](isl::set domain, isl::aff aff) {
      auto cst = aff.get_constant_val();
      auto partial = isl::pw_aff(aff.set_constant_val(cst.mul_ui(4)))
                         .intersect_domain(domain);
      result = result.union_add(partial);
      return isl_stat_ok;
    });
    scheduledAccess = scheduledAccess.set_pw_aff(i, result);
  }
  return scheduledAccess.release();
}

static std::string codegenDLTCopies(isl::ast_build astBuild, isl::ast_node node,
                                    pet_stmt *stmt) {
  if (stmt) {
    using namespace pet;
    auto schedule = isl::map::from_union_map(astBuild.get_schedule());
    auto iteratorMap = isl::pw_multi_aff::from_map(schedule.reverse());
    return printPetStmt(stmt,
                        buildRef2Expr(stmt, astBuild, transformSubscriptsDLT,
                                      iteratorMap.get()));
  }

  auto schedule = astBuild.get_schedule();
  auto original = schedule.reverse().range_factor_domain();
  auto dlt = schedule.reverse().range_factor_range();
  auto originalExpr = astBuild.access_from(
      isl::pw_multi_aff::from_map(isl::map::from_union_map(original)));
  auto dltExpr = astBuild.access_from(
      isl::pw_multi_aff::from_map(isl::map::from_union_map(dlt)));

  std::stringstream ss;
  auto direction = isl::set(schedule.domain()).get_tuple_id().get_name();
  if (direction == "from") {
    ss << originalExpr.to_C_str() << " = " << dltExpr.to_C_str() << ";";
  } else if (direction == "to") {
    ss << dltExpr.to_C_str() << " = " << originalExpr.to_C_str() << ";";
  } else {
    ISLUTILS_DIE("unknown copy direction");
  }

  return ss.str();
}

isl::schedule_node
replaceDFSPostorderOnce(isl::schedule_node node,
                        const matchers::ScheduleNodeMatcher &pattern,
                        const builders::ScheduleNodeBuilder &replacement) {
  for (int i = 0; i < node.n_children(); ++i) {
    node =
        replaceDFSPostorderOnce(node.child(i), pattern, replacement).parent();
  }
  return replaceOnce(node, pattern, replacement);
}

TEST(Transformer, HenrettyDLTJacobi) {
  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/stencil.c");
  auto scop = petScop.getScop();

  auto dependences = computeAllDependences(scop);
  isl::schedule_node node;

  auto is3Dstencil = [&](isl::schedule_node band) {
    using namespace matchers;
    // A band node always have a child (may be a leaf), and the prefix schedule
    // of that child includes the partial schedule of the node.
    auto prefixSchedule = band.child(0).get_prefix_schedule_union_map();
    auto scheduledReads = scop.reads.curry().apply_domain(prefixSchedule);
    auto arr = arrayPlaceholder();
    auto i = placeholder(ctx);

    auto matches =
        match(scheduledReads, allOf(access(dim(-1, i - 1)), access(dim(-1, i)),
                                    access(dim(-1, i + 1))));
    node = band;
    return matches.size() == 1;
  };

  auto DLTbuilder = [&]() {
    using namespace builders;

    auto dltExtensionBuilder = [&]() {
      auto prefixSchedule = node.get_prefix_schedule_union_map();
      auto scheduledReads =
          scop.reads.domain_factor_domain().apply_domain(prefixSchedule);
      // Because of the matcher, we know that only one array is accessed, so
      // untagged accesses live in the same space.
      isl::map dltMap = make1dDLT(isl::map::from_union_map(scheduledReads), 4);
      auto dlt = isl::union_map(dltMap);

      // FIXME: what if there is a dependence on schedule?
      return prefixSchedule.range().product(dlt.wrap()).unwrap();
#if 0
      auto scheduledDLT = scheduledReads.apply_range(dlt);
      return scheduledReads.range_product(scheduledDLT);
#endif
    };

    auto extensionBuilder = [dltExtensionBuilder]() {
      auto DLTExtension = dltExtensionBuilder();
      return addRangeId(DLTExtension, "to")
          .unite(addRangeId(DLTExtension, "from"));
    };

    auto toFilterBuilder = [dltExtensionBuilder]() {
      return addRangeId(dltExtensionBuilder(), "to").range().universe();
    };

    auto fromFilterBuilder = [dltExtensionBuilder]() {
      return addRangeId(dltExtensionBuilder(), "from").range().universe();
    };

    auto toBandBuilder = [dltExtensionBuilder]() {
      return isl::multi_union_pw_aff::from_union_map(
                 addRangeId(dltExtensionBuilder(), "to")
                     .range()
                     .affine_hull()
                     .wrapped_domain_map())
          .reset_tuple_id(isl::dim::set);
    };

    auto fromBandBuilder = [dltExtensionBuilder]() {
      return isl::multi_union_pw_aff::from_union_map(
                 addRangeId(dltExtensionBuilder(), "from")
                     .range()
                     .affine_hull()
                     .wrapped_domain_map())
          .reset_tuple_id(isl::dim::set);
    };

    auto coreFitlerBuilder = [&node]() { return node.get_domain(); };

    return extension(
        extensionBuilder,
        sequence(filter(toFilterBuilder, band(toBandBuilder)),
                 filter(coreFitlerBuilder, subtree([&]() {
                          return subtreeBuilder(node);
                        })), // TODO: transform the actual computation
                 filter(fromFilterBuilder, band(fromBandBuilder))));
  }();

  auto matcher = matchers::band(is3Dstencil, matchers::anyTree());
  EXPECT_TRUE(matchers::ScheduleNodeMatcher::isMatching(
      matcher, scop.schedule.get_root().child(0).child(0).child(0).child(0)));
  EXPECT_TRUE(matchers::ScheduleNodeMatcher::isMatching(
      matcher, scop.schedule.get_root().child(0).child(0).child(1).child(0)));

  scop.schedule =
      replaceDFSPostorderOnce(scop.schedule.get_root(), matcher, DLTbuilder)
          .get_schedule();
  petScop.schedule() = scop.schedule;
  static_cast<isl::schedule>(petScop.schedule()).dump();

  auto result = isl::union_map::empty(scop.reads.get_space());
  scop.reads.foreach_map([&result](isl::map m) {
    if (m.get_tuple_id(isl::dim::out).get_name() == "A") {
      m.set_tuple_name(isl::dim::out, "TEST");
    }
    result = result.add_map(m);
    return isl_stat_ok;
  });
  scop.reads = result;

  std::cout << petScop.codegen(codegenDLTCopies) << std::endl;
}

isl::schedule_node getMergedTree(isl::schedule_node root) {

  isl::schedule_node parent, child, grandchild;
  // Note that the lambda is called immediately and is only necessary for
  // compound initialization (matchers are not copyable).
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
             band(child,
               anyTree(grandchild)));
    // clang-format on
  }();

  // Use lambdas to lazily initialize the builder with the nodes and values yet
  // to be captured by the matcher.
  auto declarativeMerger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule = [&]() {
      return parent.band_get_partial_schedule().flat_range_product(
          child.band_get_partial_schedule());
    };
    auto st = [&]() { return subtreeBuilder(grandchild); };
    declarativeMerger = band(schedule, subtree(st));
  }

  // Keep transforming the tree while possible.
  auto node = root;
  while (matchers::ScheduleNodeMatcher::isMatching(matcher, node)) {
    node = node.cut();
    node = declarativeMerger.insertAt(node);
  }
  return node;
}

static isl::multi_union_pw_aff getTileSchedule(isl::schedule_node node,
                                               std::vector<int> tileSizes) {
  assert(tileSizes.size() != 0 && "empty tileSizes array");
  isl::space space = isl::manage(isl_schedule_node_band_get_space(node.get()));
  unsigned dims = space.dim(isl::dim::set);
  assert(dims == tileSizes.size() &&
         "number of dimensions should match tileSizes size");

  isl::multi_val sizes = isl::multi_val::zero(space);
  for (unsigned i = 0; i < dims; ++i) {
    int tileSize = tileSizes[i];
    sizes = sizes.set_val(i, isl::val(node.get_ctx(), tileSize));
  }

  isl::multi_union_pw_aff sched = node.band_get_partial_schedule();
  for (unsigned i = 0; i < dims; ++i) {

    isl::union_pw_aff upa = sched.get_union_pw_aff(i);
    isl::val v = sizes.get_val(i);
    upa = upa.scale_down_val(v);
    upa = upa.floor();
    sched = sched.set_union_pw_aff(i, upa);
  }
  return sched;
}

static isl::multi_union_pw_aff getPointSchedule(isl::schedule_node node,
                                                std::vector<int> tileSizes) {
  auto tileSchedule = getTileSchedule(node, tileSizes);
  auto sched = node.band_get_partial_schedule();
  return sched.sub(tileSchedule);
}

TEST(Transform, MergeAndTransform) {

  auto ctx = ScopedCtx(pet::allocCtx());
  auto petScop = pet::Scop::parseFile(ctx, "inputs/1mmWithoutInitStmt.c");
  auto scop = petScop.getScop();
  
  isl::schedule_node newRoot = 
    getMergedTree(scop.schedule.get_root()).parent();
  
  // Generic matcher/builder for tiling.
  int dims = 3;
  std::vector<int> tileSizes = {32,32,32};

  // callback to check loop dims.
  auto isNDimm = [dims](isl::schedule_node band) {
    int loopDims =
      isl_schedule_node_band_n_member(band.get());
    if (loopDims != dims)
      return false; 
    return true;
  };
  // matcher
  isl::schedule_node loop, child;
  auto matcher = [&]() {
    using namespace matchers;
    return band(isNDimm, loop, anyTree(child));
  }();
  // decl. builder
  auto builder = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    auto schedule_point = [&]() {
      auto descr = BandDescriptor(getTileSchedule(loop, tileSizes));
      descr.permutable = 1;
      return descr;
    };
    auto schedule_tile = [&]() {
      auto descr = BandDescriptor(getPointSchedule(loop, tileSizes));
      descr.permutable = 1;
      return descr;
    };
    auto st = [&]() { return subtreeBuilder(child); };
    builder = band(schedule_point, band(schedule_tile, subtree(st)));
  }
  
  if (matchers::ScheduleNodeMatcher::isMatching(matcher, newRoot.child(0)))
    newRoot = rebuild(newRoot.child(0), builder);
  std::cout << newRoot.to_str() << "\n";
  newRoot =
    getMergedTree(newRoot.parent());
  std::cout << newRoot.root().to_str() << std::endl;
}

isl::schedule_node simplifyTree(isl::schedule_node root) {

  isl::schedule_node parent, child, grandchild;
  auto matcher = [&]() {
    using namespace matchers;
    // clang-format off
    return band(parent,
      band(child,
        anyTree(grandchild)));
    //clang-format on
  }();
    
  auto merger = builders::ScheduleNodeBuilder();
  {
    using namespace builders;
    // clang-format off
    auto computeSched = [&]() {
      isl::multi_union_pw_aff sched =
        parent.band_get_partial_schedule().flat_range_product(
          child.band_get_partial_schedule());
      return sched;
    };
    // clang-format on
    auto st = [&]() { return subtreeBuilder(grandchild); };
    merger = band(computeSched, subtree(st));
  }

  root = replaceDFSPreorderRepeatedly(root, matcher, merger);

  return root.root();
}

static isl::multi_union_pw_aff getSchedulePointTile(isl::schedule_node node,
                                              std::vector<int> &s) {

  auto tile_schedule = getScheduleTile(node, s);
  auto sched = node.band_get_partial_schedule();
  return sched.sub(tile_schedule);
}
/*
TEST(Transformer, fusion) {

  auto ctx = ScopedCtx(pet::allocCtx());
  auto pet_scop =  
    pet::Scop::parseFile(ctx, "inputs/fusion.c");

  isl::schedule schedule = pet_scop.schedule();
  isl::schedule_node root = schedule.get_root();

  isl::schedule_node domain_node;
  isl::schedule_node upper_band_node, lower_band_node;
  isl::schedule_node upper_filter_node, lower_filter_node;

  auto matcher = [&]() {
    using namespace matchers;
    return 
      domain(domain_node,
        sequence(
          filter(upper_filter_node,
            band(upper_band_node, leaf())),
          filter(lower_filter_node, 
            band(lower_band_node, leaf()))));
  }();

  ASSERT_TRUE(
    matchers::ScheduleNodeMatcher::isMatching(matcher, root));

  auto m1 = upper_band_node.child(0).get_prefix_schedule_union_map();
  auto m2 = lower_band_node.child(0).get_prefix_schedule_union_map();
  auto mupa1 = isl::multi_union_pw_aff::from_union_map(m1);
  auto mupa2 = isl::multi_union_pw_aff::from_union_map(m2);
  auto fused_schedule = mupa1.union_add(mupa2);

  auto new_root = [&]() {
    using namespace builders;
  
    auto builder =
      domain(domain_node.domain_get_domain(),
        band(fused_schedule,
          sequence(filter(upper_filter_node.filter_get_filter()),
                   filter(lower_filter_node.filter_get_filter()))));
    return builder.build();
  }();

  pet_scop.schedule() = new_root.get_schedule();
  std::cout << pet_scop.codegen() << "\n";
}
*/
