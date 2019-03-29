//
// Created by andre on 09.05.16.
//

#include "DaedalusVM.h"
#include <map>
#include <assert.h>

const int NUM_FAKE_STRING_SYMBOLS = 5;

using namespace ZenLoad;
using namespace Daedalus;

DaedalusVM::DaedalusVM(const std::vector<uint8_t> data)
  :DaedalusVM(data.data(),data.size()){
  }

DaedalusVM::DaedalusVM(const uint8_t* pDATFileData, size_t numBytes)
    : m_DATFile(pDATFileData, numBytes),m_GameState(*this) {
  m_Stack.reserve(1024);
  // Make fake-strings
  for(size_t i = 0; i<NUM_FAKE_STRING_SYMBOLS; i++) {
    auto symIndex = m_DATFile.addSymbol();
    // make sure there is enough space for 1 string
    m_DATFile.getSymbolByIndex(symIndex).strData.resize(1);
    m_FakeStringSymbols.push(symIndex);
    }

  if(m_DATFile.hasSymbolName("self"))
    m_SelfId = m_DATFile.getSymbolIndexByName("self");
  m_CurrentInstanceHandle=nullptr;
  }

void DaedalusVM::eval(uint32_t PC) {
  int32_t      a=0;
  int32_t      b=0;
  uint32_t     arr=0, arr2=0;
  int32_t*     addr=nullptr;
  std::string *straddr=nullptr;

  while(true) {
    const PARStackOpCode& op = nextInstruction(PC);
    switch (op.op) {
      case EParOp_Add: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a+b);
        break;
        }
      case EParOp_Subract: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a-b);
        break;
        }
      case EParOp_Multiply: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a*b);
        break;
        }
      case EParOp_Divide:{
        a = popDataValue();
        b = popDataValue();
        if(b==0)
          terminateScript();
        pushInt(a/b);
        break;
        }
      case EParOp_Mod: {
        a = popDataValue();
        b = popDataValue();
        if(b==0)
          terminateScript();
        pushInt(a%b);
        break;
        }
      case EParOp_BinOr: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a | b);
        break;
        }
      case EParOp_BinAnd: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a & b);
        break;
        }
      case EParOp_Less: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a < b ? 1 : 0);
        break;
        }
      case EParOp_Greater: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a > b ? 1 : 0);
        break;
        }

      case EParOp_AssignFunc:
      case EParOp_Assign: {
        size_t  a = popVar(arr);
        int32_t b = popDataValue();

        int32_t& aInt = m_DATFile.getSymbolByIndex(a).getInt(arr, getCurrentInstanceDataPtr());
        aInt = b;
        break;
        }
      case EParOp_LogOr:
        a = popDataValue();
        b = popDataValue();
        pushInt(a | b ? 1 : 0);
        break;
      case EParOp_LogAnd:
        a = popDataValue();
        b = popDataValue();
        pushInt(a & b ? 1 : 0);
        break;
      case EParOp_ShiftLeft: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a << b);
        break;
        }
      case EParOp_ShiftRight: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a >> b);
        break;
        }
      case EParOp_LessOrEqual: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a <= b ? 1 : 0);
        break;
        }
      case EParOp_Equal: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a == b ? 1 : 0);
        break;
        }
      case EParOp_NotEqual: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a != b ? 1 : 0);
        break;
        }
      case EParOp_GreaterOrEqual: {
        a = popDataValue();
        b = popDataValue();
        pushInt(a >= b ? 1 : 0);
        break;
        }
      case EParOp_AssignAdd:{
        size_t v = popVar(arr);
        addr   = &m_DATFile.getSymbolByIndex(v).getInt(arr, getCurrentInstanceDataPtr());
        *addr += popDataValue();
        break;
        }
      case EParOp_AssignSubtract: {
        size_t v = popVar(arr);
        addr   = &m_DATFile.getSymbolByIndex(v).getInt(arr, getCurrentInstanceDataPtr());
        *addr -= popDataValue();
        break;
        }
      case EParOp_AssignMultiply: {
        size_t v = popVar(arr);
        addr = &m_DATFile.getSymbolByIndex(v).getInt(arr, getCurrentInstanceDataPtr());
        *addr *= popDataValue();
        break;
        }
      case EParOp_AssignDivide: {
        size_t v = popVar(arr);
        addr = &m_DATFile.getSymbolByIndex(v).getInt(arr, getCurrentInstanceDataPtr());
        *addr /= popDataValue();
        break;
        }
      case EParOp_Plus:
        pushInt(+popDataValue());
        break;
      case EParOp_Minus:
        pushInt(-popDataValue());
        break;
      case EParOp_Not:
        pushInt(!popDataValue());
        break;
      case EParOp_Negate:
        pushInt(~popDataValue());
        break;
      case EParOp_Ret:
        return;
      case EParOp_Call: {
        void*          currentInstanceHandle = m_CurrentInstanceHandle;
        EInstanceClass currentInstanceClass  = m_CurrentInstanceClass;

        {
        CallStackFrame frame(*this, op.address, CallStackFrame::Address);
        eval(size_t(op.address));
        }

        m_CurrentInstanceHandle = currentInstanceHandle;
        m_CurrentInstanceClass = currentInstanceClass;
        break;
        }

      case EParOp_CallExternal: {
        std::function<void(DaedalusVM&)>* f=nullptr;
        if(size_t(op.symbol)<m_ExternalsByIndex.size()){
          f = &m_ExternalsByIndex[size_t(op.symbol)];
          }
        //auto it = m_ExternalsByIndex.find(size_t(op.symbol));

        void*          currentInstanceHandle = m_CurrentInstanceHandle;
        EInstanceClass currentInstanceClass  = m_CurrentInstanceClass;

        if(f!=nullptr && *f) {
          CallStackFrame frame(*this, op.symbol, CallStackFrame::SymbolIndex);
          (*f)(*this);
          } else {
          CallStackFrame frame(*this, op.symbol, CallStackFrame::SymbolIndex);
          m_OnUnsatisfiedCall(*this);
          }

        m_CurrentInstanceHandle = currentInstanceHandle;
        m_CurrentInstanceClass = currentInstanceClass;
        break;
        }

      case EParOp_PushInt:
        pushInt(op.value);
        break;
      case EParOp_PushVar:
        pushVar(size_t(op.symbol));
        break;
      case EParOp_PushInstance:
        pushVar(size_t(op.symbol));
        break;  //TODO: Not sure about this
      case EParOp_AssignString: {
        size_t a = popVar(arr);
        size_t b = popVar(arr2);

        straddr         = &m_DATFile.getSymbolByIndex(a).getString(arr,  getCurrentInstanceDataPtr());
        std::string& s2 =  m_DATFile.getSymbolByIndex(b).getString(arr2, getCurrentInstanceDataPtr());
        *straddr = s2;
        break;
        }

      case EParOp_AssignStringRef:
        LogError() << "EParOp_AssignStringRef not implemented!";
        break;

      case EParOp_AssignFloat: {
        size_t a = popVar(arr);
        float  b = popFloatValue();
        float& aFloat = m_DATFile.getSymbolByIndex(a).getFloat(arr, getCurrentInstanceDataPtr());
        aFloat = b;
        break;
        }

      case EParOp_AssignInstance: {
        size_t a = popVar();
        size_t b = popVar();

        m_DATFile.getSymbolByIndex(a).instanceDataHandle = m_DATFile.getSymbolByIndex(b).instanceDataHandle;
        m_DATFile.getSymbolByIndex(a).instanceDataClass  = m_DATFile.getSymbolByIndex(b).instanceDataClass;
        break;
        }

      case EParOp_Jump:
        PC = size_t(op.address);
        break;

      case EParOp_JumpIf:
        a = popDataValue();
        if(a==0)
          PC = size_t(op.address);
        break;

      case EParOp_SetInstance:
        m_CurrentInstance = size_t(op.symbol);
        setCurrentInstance(size_t(op.symbol));
        break;
      case EParOp_PushArrayVar:
        pushVar(size_t(op.symbol), op.index);
        break;
      }
    }
  }

const PARStackOpCode& DaedalusVM::nextInstruction(size_t& pc) {
  const PARStackOpCode& op = m_DATFile.getStackOpCode(pc);
  pc += op.opSize;
  return op;
  }

void DaedalusVM::registerExternalFunction(const char* symName, const std::function<void(DaedalusVM&)>& fn) {
  if(m_DATFile.hasSymbolName(symName)) {
    size_t s = m_DATFile.getSymbolIndexByName(symName);
    if(m_ExternalsByIndex.size()<=s)
      m_ExternalsByIndex.resize(s+1);
    m_ExternalsByIndex[s] = fn;
    }
  }

void DaedalusVM::registerUnsatisfiedLink(const std::function<void (DaedalusVM &)> &fn) {
  m_OnUnsatisfiedCall = fn;
  }

void DaedalusVM::pushInt(uint32_t value) {
  m_Stack.emplace_back(reinterpret_cast<int32_t&>(value));
  }

void DaedalusVM::pushInt(int32_t value) {
  m_Stack.emplace_back(value);
  }

template <typename T>
T DaedalusVM::popDataValue() {
  // Default behavior of the ZenGin is to pop a 0, if nothing is on the stack.
  if(m_Stack.empty())
    return static_cast<T>(0);

  auto top = m_Stack.back();
  m_Stack.pop_back();
  if(top.tag==EParOp_PushVar){
    return m_DATFile.getSymbolByIndex(size_t(top.i32)).getValue<T>(top.id, getCurrentInstanceDataPtr());
    }
  return top.i32;
  }

void DaedalusVM::pushVar(size_t index, uint32_t arrIdx) {
  m_Stack.emplace_back(index,arrIdx);
  }

uint32_t DaedalusVM::popVar(uint32_t& arrIdx) {
  if(m_Stack.empty()){
    arrIdx=0;
    return 0;
    }

  auto top = m_Stack.back();
  m_Stack.pop_back();
  arrIdx = top.id;
  return uint32_t(top.i32);
  }

std::string DaedalusVM::popString(bool toUpper) {
  uint32_t arr;
  uint32_t idx = popVar(arr);

  std::string s = m_DATFile.getSymbolByIndex(idx).getString(arr, getCurrentInstanceDataPtr());

  if (toUpper)
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);

  return s;
  }

void DaedalusVM::setReturn(int32_t v) {
  pushInt(v);
  }

void DaedalusVM::setReturn(const std::string& v) {
  pushString(v);
  }

void DaedalusVM::setReturn(float f) {
  m_Stack.emplace_back(f);
  }

void DaedalusVM::setReturnVar(int32_t v) {
  pushVar(size_t(v));
  }

int32_t DaedalusVM::popInt() {
  if(m_Stack.empty())
    return 0;

  auto top = m_Stack.back();
  m_Stack.pop_back();
  if(top.tag==EParOp_PushVar){
    return m_DATFile.getSymbolByIndex(size_t(top.i32)).getInt(top.id, getCurrentInstanceDataPtr());
    }
  return top.i32;
  }

float DaedalusVM::popFloat() {
  if(m_Stack.empty())
    return 0;

  auto top = m_Stack.back();
  m_Stack.pop_back();
  if(top.tag==EParOp_PushVar){
    return m_DATFile.getSymbolByIndex(size_t(top.i32)).getFloat(top.id, getCurrentInstanceDataPtr());
    }
  return top.f;
  }

float DaedalusVM::popFloatValue() {
  auto top = m_Stack.back();
  m_Stack.pop_back();
  return top.f;
  }

uint32_t DaedalusVM::popVar() {
  uint32_t arr=0;
  return popVar(arr);
  }

void DaedalusVM::pushVar(const char* symName) {
  size_t idx = m_DATFile.getSymbolIndexByName(symName);
  pushVar(idx);
  }

void DaedalusVM::pushString(const std::string& str) {
  size_t symIdx = m_FakeStringSymbols.front();
  Daedalus::PARSymbol& s = m_DATFile.getSymbolByIndex(symIdx);
  m_FakeStringSymbols.push(m_FakeStringSymbols.front());
  m_FakeStringSymbols.pop();

  s.getString(0) = str;

  pushVar(symIdx, 0);
  }

void DaedalusVM::setInstance(const char* instSymbol, void* h, EInstanceClass instanceClass) {
  PARSymbol& s = m_DATFile.getSymbolByName(instSymbol);
  s.instanceDataHandle = h;
  s.instanceDataClass  = instanceClass;
  }

void DaedalusVM::initializeInstance(void *instance, size_t symIdx, EInstanceClass classIdx) {
  PARSymbol& s = m_DATFile.getSymbolByIndex(symIdx);

  // Enter address into instance-symbol
  s.instanceDataHandle = instance;
  s.instanceDataClass  = classIdx;

  void*          currentInstanceHandle = m_CurrentInstanceHandle;
  EInstanceClass currentInstanceClass  = m_CurrentInstanceClass;

  setCurrentInstance(symIdx);

  PARSymbol selfCpy;
  // Particle and Menu VM do not have a self symbol
  if(m_SelfId!=size_t(-1)) {
    selfCpy = m_DATFile.getSymbolByIndex(m_SelfId);  // Copy of "self"-symbol
    // Set self
    setInstance("self", instance, classIdx);
    }

  // Place the assigning symbol into the instance
  GEngineClasses::Instance* instData = m_GameState.getByClass(instance, classIdx);
  instData->instanceSymbol = symIdx;

  // Run script code to initialize the object
  runFunctionBySymIndex(symIdx);

  if(m_SelfId!=size_t(-1))
    m_DATFile.getSymbolByIndex(m_SelfId) = selfCpy;

  m_CurrentInstanceHandle = currentInstanceHandle;
  m_CurrentInstanceClass  = currentInstanceClass;
  }

void DaedalusVM::setCurrentInstance(size_t symIdx) {
  m_CurrentInstance       = symIdx;
  m_CurrentInstanceHandle = m_DATFile.getSymbolByIndex(symIdx).instanceDataHandle;
  m_CurrentInstanceClass  = m_DATFile.getSymbolByIndex(symIdx).instanceDataClass;
  }

void* DaedalusVM::getCurrentInstanceDataPtr() {
  return m_GameState.getByClass(m_CurrentInstanceHandle, m_CurrentInstanceClass);
  }

std::vector<std::string> DaedalusVM::getCallStack() {
  std::vector<std::string> symbolNames;
  auto frame=m_CallStack;
  while(frame) {
    symbolNames.push_back(nameFromFunctionInfo(std::make_pair(frame->addressOrIndex, frame->addrType)));
    frame = frame->calee;
    }
  return symbolNames;
  }

const std::string &DaedalusVM::currentCall() {
  if(m_CallStack)
    return nameFromFunctionInfo(std::make_pair(m_CallStack->addressOrIndex, m_CallStack->addrType));

  static std::string n = "<no function>";
  return n;
  }

int32_t DaedalusVM::runFunctionBySymIndex(size_t symIdx, bool clearDataStack) {
  if(symIdx==size_t(-1))
    return 0;

  if(clearDataStack)
    m_Stack.clear();

  CallStackFrame frame(*this, int32_t(symIdx), CallStackFrame::SymbolIndex);
  auto& functionSymbol = getDATFile().getSymbolByIndex(symIdx);
  auto& address = functionSymbol.address;
  if(address == 0)
    return -1;

  // Execute the instructions
  eval(address);

  int32_t ret = 0;

  if(functionSymbol.hasEParFlag(EParFlag_Return)) {
    // Only pop if the VM didn't mess up
    if(m_Stack.size()>0)
      ret = popDataValue();  // TODO handle vars?
    }
  return ret;
  }

const std::string& DaedalusVM::nameFromFunctionInfo(DaedalusVM::CallStackFrame::FunctionInfo functionInfo) {
  switch(functionInfo.second) {
    case CallStackFrame::SymbolIndex: {
      auto functionSymbolIndex = functionInfo.first;
      return getDATFile().getSymbolByIndex(functionSymbolIndex).name;
      }
    case CallStackFrame::Address: {
      auto address = functionInfo.first;
      auto functionSymbolIndex = getDATFile().getFunctionIndexByAddress(address);
      if (functionSymbolIndex != static_cast<size_t>(-1))
        return getDATFile().getSymbolByIndex(functionSymbolIndex).name;
      }
      }
  static std::string err;
  err = "unknown function with address: " + std::to_string(functionInfo.first);
  return err;
  }

[[noreturn]]
void DaedalusVM::terminateScript(){
  throw std::logic_error("fatal script error");
  }

DaedalusVM::CallStackFrame::CallStackFrame(DaedalusVM& vm, int32_t addressOrIndex, AddressType addrType)
    : calee(vm.m_CallStack), addressOrIndex(addressOrIndex), addrType(addrType), vm(vm) {
  // entering function
  vm.m_CallStack = this;
  }

DaedalusVM::CallStackFrame::~CallStackFrame() {
  vm.m_CallStack = calee;
  }
