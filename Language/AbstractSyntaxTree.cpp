#include "AbstractSyntaxTree.hpp"

llvm::Value* AST::Number::codegen()
{
	//std::cout << "CodeGen Number...\n";

	if(isDouble)
		return llvm::ConstantFP::get(*CodeGeneration::TheContext, llvm::APFloat(doubleValue));
	else if(isFloat)
		return llvm::ConstantFP::get(*CodeGeneration::TheContext, llvm::APFloat(floatValue));
	else if(isInt)
	{
		//return llvm::ConstantFP::get(*CodeGeneration::TheContext, llvm::APFloat((double)intValue));

		llvm::Type *i32_type = llvm::IntegerType::getInt32Ty(*CodeGeneration::TheContext);
		//CodeGeneration::lastPureInt = intValue;
		return llvm::ConstantInt::get(i32_type, intValue, true);
	}

	return llvm::ConstantFP::get(*CodeGeneration::TheContext, llvm::APFloat(doubleValue));
}

llvm::Value* AST::Integer::codegen()
{
	//std::cout << "CodeGen Integer...\n";
	CodeGeneration::isPureNumber = false;
	return llvm::ConstantInt::get(*CodeGeneration::TheContext, llvm::APInt(32, value, true));
}

llvm::Value* AST::Float::codegen()
{
	//std::cout << "CodeGen Float...\n";
	CodeGeneration::isPureNumber = false;
	return llvm::ConstantFP::get(*CodeGeneration::TheContext, llvm::APFloat(value));
}

llvm::Value* AST::Double::codegen()
{
	//std::cout << "CodeGen Double...\n";
	CodeGeneration::isPureNumber = false;
	return llvm::ConstantFP::get(*CodeGeneration::TheContext, llvm::APFloat(value));
}

llvm::Value* AST::Variable::codegen()
{
	//std::cout << "CodeGen Variable...\n";

	llvm::Value* V = CodeGeneration::NamedValues[name];
	CodeGeneration::isPureNumber = false;

	if(!V)
		CodeGeneration::LogErrorV("Unknown variable name.\n");

	return V;
}

llvm::Value* AST::Binary::codegen()
{
	//std::cout << "CodeGen Binary...\n";

	std::vector<int> pureNumbers;

	llvm::Value* L = lhs->codegen();
	llvm::Value* R = rhs->codegen();

	llvm::Value* opLLVM = nullptr;

	if(!L || !R)
	{
		std::cout << "Warning: One of the Values is nullptr.\n";
		return nullptr;
	}

	if(op == '+')
	{
		if(static_cast<llvm::ConstantInt*>(L) != nullptr && static_cast<llvm::ConstantInt*>(R) != nullptr)
		{
			opLLVM = CodeGeneration::Builder->CreateAdd(L, R, "addtmp");
		}
		else
		{
			opLLVM = CodeGeneration::Builder->CreateFAdd(L, R, "addtmp");
		}
	}
	else if(op == '-')
	{
		if(static_cast<llvm::ConstantInt*>(L) != nullptr && static_cast<llvm::ConstantInt*>(R) != nullptr)
		{
			opLLVM = CodeGeneration::Builder->CreateSub(L, R, "subtmp");
		}
		else
		{
			opLLVM = CodeGeneration::Builder->CreateFSub(L, R, "subtmp");
		}
	}
	else if(op == '*')
	{
		if(static_cast<llvm::ConstantInt*>(L) != nullptr && static_cast<llvm::ConstantInt*>(R) != nullptr)
		{
			opLLVM = CodeGeneration::Builder->CreateMul(L, R, "multmp");
		}
		else
		{
			opLLVM = CodeGeneration::Builder->CreateFMul(L, R, "multmp");
		}
	}
		//case '<':
		//	L = CodeGeneration::Builder->CreateFCmpULT(L, R, "cmptmp");
		//	return CodeGeneration::Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*CodeGeneration::TheContext), "booltmp");

		//default:

	if(opLLVM == nullptr)
		return CodeGeneration::LogErrorV("Invalid binary operator (" + std::to_string(op) + ").\n");

	CodeGeneration::lastLLVMInOp = opLLVM;

	return opLLVM;
}

llvm::Value* AST::Call::codegen()
{
	//std::cout << "CodeGen Call...\n";

	llvm::Function* CalleeF = CodeGeneration::TheModule->getFunction(callee);
	if(!CalleeF)
		return CodeGeneration::LogErrorV("Unknown function referenced.\n");

	if(CalleeF->arg_size() != arguments.size())
		return CodeGeneration::LogErrorV("Incorrect # arguments passed.\n");

	std::vector<llvm::Value*> ArgsV;
	for(unsigned i = 0, e = arguments.size(); i != e; ++i)
	{
		ArgsV.push_back(arguments[i]->codegen());
		if(!ArgsV.back())
			return nullptr;
	}

	return CodeGeneration::Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Function* AST::FunctionPrototype::codegen()
{
	//std::cout << "CodeGen Prototype...\n";

	std::vector<llvm::Type*> llvmArgs;

	for(auto const& i: arguments)
	{
		if(dynamic_cast<Double*>(i.first.get()) != nullptr)
			llvmArgs.push_back(llvm::Type::getDoubleTy(*CodeGeneration::TheContext));
		else if(dynamic_cast<Integer*>(i.first.get()) != nullptr)
			llvmArgs.push_back(llvm::Type::getInt32Ty(*CodeGeneration::TheContext));
		else if(dynamic_cast<Float*>(i.first.get()) != nullptr)
			llvmArgs.push_back(llvm::Type::getFloatTy(*CodeGeneration::TheContext));
	}

	llvm::FunctionType* FT = nullptr;

	if(dynamic_cast<Double*>(type.get()) != nullptr)
		FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*CodeGeneration::TheContext), llvmArgs, false);
	else if(dynamic_cast<Integer*>(type.get()) != nullptr)
		FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(*CodeGeneration::TheContext), llvmArgs, false);
	else if(dynamic_cast<Float*>(type.get()) != nullptr)
		FT = llvm::FunctionType::get(llvm::Type::getFloatTy(*CodeGeneration::TheContext), llvmArgs, false);
	else
		return CodeGeneration::LogErrorFLLVM("Unknown function type.");

	if(FT == nullptr)
		return CodeGeneration::LogErrorFLLVM("FT is nullptr.");

	llvm::Function* F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, CodeGeneration::TheModule.get());

	unsigned Idx = 0;
	for (auto &Arg : F->args())
  		Arg.setName(arguments[Idx++].second->name);

	return F;
}

llvm::Function* AST::Function::codegen()
{
	//std::cout << "CodeGen Function...\n";

	llvm::Function* TheFunction = CodeGeneration::TheModule->getFunction(prototype->Name());

	if(!TheFunction)
		TheFunction = prototype->codegen();

	if(!TheFunction)
		return nullptr;

	if(!TheFunction->empty())
		return (llvm::Function*)CodeGeneration::LogErrorV("Function cannot be redefined.\n");

	// Basic block start
	llvm::BasicBlock* BB = llvm::BasicBlock::Create(*CodeGeneration::TheContext, "entry", TheFunction);
	CodeGeneration::Builder->SetInsertPoint(BB);

	CodeGeneration::NamedValues.clear();

	for(auto& A : TheFunction->args())
	{
		auto getNameOfA = std::string(A.getName());
		CodeGeneration::NamedValues[getNameOfA] = &A;
	}

	if(llvm::Value* RetVal = body->codegen())
	{
		CodeGeneration::Builder->CreateRet(RetVal);

		llvm::verifyFunction(*TheFunction);

		CodeGeneration::TheFPM->run(*TheFunction);

		return TheFunction;
	}

	TheFunction->eraseFromParent();
	return nullptr;
}