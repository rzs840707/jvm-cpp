#include "Jvm.h"
#include "classpath.h"
#include <iostream>
#include "ClassFile.h"

void Jvm::StartJvm(std::map<std::string, docopt::value> args)
{
	std::cout << "Starting JVM: " << args["<target>"] << std::endl;
	std::cout << "Xjre: " << args["--Xjre"] << std::endl;
	auto cp = new Classpath(args);
	std::cout << cp->String() << std::endl;
	auto target = args["<target>"].asString();
	std::replace(target.begin(), target.end(), '.', boost::filesystem::path::preferred_separator);
	auto result = cp->ReadClass(target);
	if (result.status == STATUS_OK)
	{
		std::cout << "data("<< result.data.size() <<"): " << std::endl;
		for (auto i: result.data)
		{
			printf("%X", i);
		}
		std::cout << std::endl;
		
		auto classFile = new ClassFile();
		auto parseResult = classFile->Parse(result.data);
		if(parseResult.status == STATUS_ERR)
		{
			std::cout << parseResult.error << std::endl;
		}
	} else
	{
		std::cout << "StartJVM error: " << result.err.what() << std::endl;
	}
}
