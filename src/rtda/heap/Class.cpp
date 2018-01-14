//
// Created by 李晨曦 on 11/12/2017.
//

#include "Class.hpp"

std::string ObjectClass = "java/lang/Object";

std::vector<std::string> IntTypesStr = std::vector<std::string>({"Z", "B", "C", "S", "I"});

std::vector<byte> IntTypesByte = std::vector<byte>({'Z', 'B', 'C', 'S', 'I'});

Class::Class(ClassFile *cf)
: accessFlag(cf->AccessFlags()), name(cf->ClassName()), superClassName(cf->SuperClassName()),
  interfaceNames(cf->InterfaceName()), superClass(nullptr),
  constantPool(new RunTimeConstantPool(this, *cf->GetConstantPool())), fields(load<Field>(cf->Fields())),
  methods(load<Method>(cf->Methods())) {}

ClassMember::ClassMember(MemberInfo *memberInfo, Class *belongClass)
: accessFlags(memberInfo->AccessFlags()), name(memberInfo->Name()), descriptor(memberInfo->Descriptor()),
  belongClass(belongClass) {}

void FieldRef::resolveFieldRef() {
	auto mainClass = rtcp->belongClass;
	auto newClass = mainClass->getLoader()->LoadClass(className);
	field = lookupField(newClass, name, description);
	if (field == nullptr) {
		throw JavaFieldNotExistException();
	} else if (!field->IsAccessibleTo(mainClass)) {
		throw JavaIllegalAccessException();
	}
}

Field *FieldRef::lookupField(Class *owner, std::string name, std::string descriptor) {
	for (auto field : owner->getFields()) {
		if (field->name == name && field->descriptor == descriptor) {
			return field;
		}
	}
	for (auto interface: owner->getInterfaces()) {
		auto field = lookupField(interface, name, descriptor);
		if (field != nullptr) {
			return field;
		}
	}
	if (owner->getSuperClass() != nullptr) {
		return lookupField(owner->getSuperClass(), name, descriptor);
	}
	return nullptr;
}

bool ClassMember::IsAccessibleTo(Class *otherClass) {
	if (IsPublic()) {
		return true;
	}
	if (IsProtected()) {
		return otherClass == belongClass || otherClass->IsSubClassOf(belongClass)
		|| otherClass->getPackageName() == belongClass->getPackageName();
	}
	if (IsPrivate()) {
		return otherClass->getPackageName() == belongClass->getPackageName();
	}
	return otherClass == belongClass;
}

Field::Field(MemberInfo *memberInfo, Class *belongClass) : ClassMember(memberInfo, belongClass),
                                                           constValueIndex(memberInfo->getConstantValueAttribute() !=
                                                           nullptr
                                                                           ? memberInfo->getConstantValueAttribute()->ConstantValueIndex()
                                                                           : 0) {}


Method::Method(MemberInfo *memberInfo,
Class *belongClass
) : ClassMember(memberInfo, belongClass), maxStack(memberInfo->getCodeAttribute() != nullptr ? memberInfo->getCodeAttribute()->getMaxStack() : 0),
    maxLocals(memberInfo->getCodeAttribute() != nullptr ? memberInfo->getCodeAttribute()->getMaxLocals() : 0),
    code(memberInfo->getCodeAttribute() != nullptr ? memberInfo->getCodeAttribute()->getCode() : std::vector<byte>(0)) {}


ClassLoader::ClassLoader(Classpath *cp) : cp(cp), classMap(std::map<std::string, Class *>()) {}

ReadClassResult ClassLoader::ReadClass(std::string className) {
	auto result = cp->ReadClass(className);
	if (result.status == STATUS_ERR) {
		throw JavaClassNotFoundException(className);
	}
	return result;
}

Class *ClassLoader::loadNoArrayClass(std::string className) {
	auto result = ReadClass(className);
	auto newClass = defineClass(result.data);
	link(newClass);
	std::cout << "Loaded " << className << " from " << result.entry;
	return newClass;
}

Class *ClassLoader::LoadClass(std::string className) {
	if (classMap.find(className) != classMap.end()) {
		return classMap[className];
	}
	return loadNoArrayClass(className);
	
}

Class *ClassLoader::defineClass(std::vector<byte> data) {
	auto newClass = parseClass(move(data));
	newClass->setLoader(this);
	resolveSuperClass(newClass);
	resolveInterfaces(newClass);
	classMap[newClass->getName()] = newClass;
	return newClass;
}

Class *ClassLoader::parseClass(std::vector<byte> data) {
	auto newClass = new ClassFile();
	auto Result = newClass->Parse(std::move(data));
	if (Result.status == STATUS_ERR) {
		throw JavaClassFormatError(Result.error);
	}
	return new Class(newClass);
}

void ClassLoader::resolveSuperClass(Class *itemClass) {
	if (itemClass->getSuperClassName() != ObjectClass) {
		itemClass->setSuperClass(itemClass->getLoader()->LoadClass(itemClass->getSuperClassName()));
	}
}

void ClassLoader::resolveInterfaces(Class *itemClass) {
	auto interfacesCount = itemClass->getInterfaceNames().size();
	if (interfacesCount > 0) {
		auto interfaces = std::vector<Class *>(0);
		for (auto &name : itemClass->getInterfaceNames()) {
			interfaces.push_back(itemClass->getLoader()->LoadClass(name));
		}
	}
}

void ClassLoader::link(Class *newClass) {
	verify(newClass);
	prepare(newClass);
}

void ClassLoader::verify(Class *newClass) {

}

void ClassLoader::prepare(Class *newClass) {
	calcInstanceFieldSlotsId(newClass);
	calcStaticFieldSlotsId(newClass);
	allocAndInitStaticVars(newClass);
}

void ClassLoader::calcInstanceFieldSlotsId(Class *newClass) {
	uint32_t slotId = 0;
	if (newClass->getSuperClass() != nullptr) {
		slotId = newClass->getSuperClass()->getInstanceSlotCount();
	}
	for (auto field: newClass->getFields()) {
		if (!field->IsStatic()) {
			field->slotId = slotId;
			slotId++;
			if (field->IsLongOrDouble()) {
				slotId++;
			}
		}
	}
	newClass->setInstanceSlotCount(slotId);
}

void ClassLoader::calcStaticFieldSlotsId(Class *newClass) {
	uint32_t slotId = 0;
	for (auto field: newClass->getFields()) {
		if (field->IsStatic()) {
			field->slotId = slotId;
			slotId++;
			if (field->IsLongOrDouble()) {
				slotId++;
			}
		}
	}
	newClass->setStaticSlotCount(slotId);
}

void ClassLoader::allocAndInitStaticVars(Class *newClass) {
	newClass->setStaticVars(new LocalVars(newClass->getStaticSlotCount()));
	
}

void ClassLoader::initStaticFinalVar(Class *newClass, Field *field) {
	auto vars = newClass->getStaticVars();
	auto constPool = newClass->getConstantPool();
	auto cpIndex = field->constValueIndex;
	auto slotId = field->slotId;
	auto descriptor = field->descriptor;
	if (cpIndex > 0) {
		if (std::find(IntTypesStr.begin(), IntTypesStr.end(), descriptor) != IntTypesStr.end()) {
			auto val = constPool->getConstant(cpIndex);
			vars->SetInt(slotId, boost::any_cast<int32_t>(val));
		} else if (descriptor == "J") {
			auto val = constPool->getConstant(cpIndex);
			vars->SetLong(slotId, boost::any_cast<int64_t>(val));
		} else if (descriptor == "F") {
			auto val = constPool->getConstant(cpIndex);
			vars->SetFloat(slotId, boost::any_cast<float>(val));
		} else if (descriptor == "D") {
			auto val = constPool->getConstant(cpIndex);
			vars->SetDouble(slotId, boost::any_cast<double>(val));
		} else if (descriptor == "Ljava/lang/String") {
			// TODO
		}
	}
	
}

RunTimeConstantPool::RunTimeConstantPool(Class *belongClass, ConstantPool constPool) : belongClass(
belongClass), consts(std::vector<Constant>(0)) {
	using namespace ConstantInfoSpace;
	for (auto constInfo: constPool.Info) {
		if (constInfo == nullptr) {
			continue;
		}
		switch (constInfo->getTag()) {
			case CONSTANT_Integer:
				consts.emplace_back(((ConstantIntegerInfo *) (constInfo))->getValue());
				break;
			case CONSTANT_Float:
				consts.emplace_back(((ConstantFloatInfo *) (constInfo))->getValue());
				break;
			case CONSTANT_Long:
				consts.emplace_back(((ConstantLongInfo *) (constInfo))->getValue());
				break;
			case CONSTANT_Double:
				consts.emplace_back(((ConstantDoubleInfo *) (constInfo))->getValue());
				break;
			case CONSTANT_String:
				consts.emplace_back(std::string(((ConstantStringInfo *) (constInfo))->String()));
				break;
			case CONSTANT_Class:
				consts.emplace_back(new ClassRef(this, ((ConstantClassInfo *) (constInfo))));
				break;
			case CONSTANT_Fieldref:
				consts.emplace_back(new FieldRef(this, ((ConstantFieldrefInfo *) (constInfo))));
				break;
			case CONSTANT_Methodref:
				consts.emplace_back(new MemberRef(this, ((ConstantMethodrefInfo *) (constInfo))));
				break;
			case CONSTANT_InterfaceMethodref:
				consts.emplace_back(new InterfaceMethodRef(this, ((ConstantInterfaceMethodrefInfo *) (constInfo))));
				break;
			default:
				break;
		}
	}
}

SymRef::SymRef(RunTimeConstantPool *rtcp, std::string className, Class *ownClass)
: rtcp(rtcp), className(className), ownClass(ownClass) {}

void SymRef::resolveClassRef() {
	auto mainClass = rtcp->belongClass;
	auto newClass = mainClass->getLoader()->LoadClass(className);
	if (!newClass->IsAccessibleTo(mainClass)) {
		throw JavaIllegalAccessException();
	}
	ownClass = newClass;
}

Class *SymRef::ResolveClass() {
	if (ownClass == nullptr) {
		resolveClassRef();
	}
	return ownClass;
}


Constant RunTimeConstantPool::getConstant(uint32_t index) {
	return consts[index];
}

ClassRef::ClassRef(RunTimeConstantPool *rtcp, ConstantClassInfo *classInfo) : SymRef(rtcp,
                                                                                     classInfo->Name(),
                                                                                     nullptr) {}

MemberRef::MemberRef(RunTimeConstantPool *rtcp, ConstantMemberrefInfo *readInfo) : SymRef(
rtcp,
readInfo->ClassName(),
nullptr), name(readInfo->NameAndType()["name"]), description(readInfo->NameAndType()["type"]) {}

FieldRef::FieldRef(RunTimeConstantPool *rtcp,
ConstantFieldrefInfo *redInfo) : MemberRef(rtcp, redInfo), field(nullptr) {}

Field *FieldRef::ResolveField() {
	if (field == nullptr) {
		resolveFieldRef();
	}
	return field;
}

MethodRef::MethodRef(RunTimeConstantPool *rtcp, ConstantMethodrefInfo *redInfo)
: MemberRef(rtcp, redInfo), method(nullptr) {}

InterfaceMethodRef::InterfaceMethodRef(RunTimeConstantPool *rtcp, ConstantInterfaceMethodrefInfo *redInfo)
: MemberRef(rtcp, redInfo), method(nullptr) {}


bool Class::IsSubClassOf(Class *otherClass) {
	auto super = getSuperClass();
	if (super == otherClass) {
		return true;
	}
	if (super == nullptr) {
		return false;
	}
	return super->IsSubClassOf(otherClass);
}

bool Class::IsImplements(Class *otherClass) {
	for (auto iface: interfaces) {
		if (iface == otherClass || iface->IsSubInterfaceOf(otherClass)) {
			return true;
		}
	}
	auto super = getSuperClass();
	if (super == nullptr) {
		return false;
	}
	return super->IsImplements(otherClass);
}

bool Class::IsSubInterfaceOf(Class *otherClass) {
	for (auto superInterface: interfaces) {
		if (superInterface == otherClass || superInterface->IsSubInterfaceOf(otherClass)) {
			return true;
		}
	}
	return false;
}


LocalVars::LocalVars(uint32_t maxLocals) {
	this->slots = std::vector<Slot *>(maxLocals);
	for (uint32_t i = 0; i < maxLocals; i++) {
		this->slots[i] = new Slot(0);
	}
}


Object::Object(Class *ownClass) : ownClass(ownClass), fields(new LocalVars(ownClass->getInstanceSlotCount())) {

}

Slot::Slot(int32_t num) : num(num) {}

Slot::Slot(Object *ref) : ref(ref) {}