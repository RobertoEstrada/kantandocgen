#pragma once
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "DocGenOutput.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "OutputFormats/DocGenSerializerFactory.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/SharedPointer.h"

#include "DocGenJsonSerializer.generated.h"

class DocGenJsonSerializer : public DocTreeNode::IDocTreeSerializer
{
	TSharedPtr<FJsonValue> TopLevelObject;
	TSharedPtr<FJsonValue>& TargetObject;

	virtual FString EscapeString(const FString& InString) override
	{
		return InString;
	}

	virtual void SerializeObject(const DocTreeNode::Object& Obj) override
	{
		TArray<FString> ObjectFieldNames;
		// If we have a single key...
		if (Obj.GetKeys(ObjectFieldNames) == 1)
		{
			TArray<TSharedPtr<DocTreeNode>> ArrayFieldValues;
			// If that single key has multiple values...
			Obj.MultiFind(ObjectFieldNames[0], ArrayFieldValues, true);
			if (ArrayFieldValues.Num() > 1)
			{
				// we are an array
				TargetObject = SerializeArray(ArrayFieldValues);
				return;
			}
		}

		if (!TargetObject.IsValid())
		{
			TargetObject = MakeShared<FJsonValueObject>(MakeShared<FJsonObject>());
		}
		for (auto& FieldName : ObjectFieldNames)
		{
			TArray<TSharedPtr<DocTreeNode>> ArrayFieldValues;
			Obj.MultiFind(ObjectFieldNames[0], ArrayFieldValues, true);
			if (ArrayFieldValues.Num() > 1)
			{
				TargetObject->AsObject()->SetField(FieldName, SerializeArray(ArrayFieldValues));
			}
			else
			{
				TSharedPtr<FJsonValue> NewMemberValue;
				ArrayFieldValues[0]->SerializeWith(MakeShared<DocGenJsonSerializer>(NewMemberValue));
				TargetObject->AsObject()->SetField(FieldName, NewMemberValue);
			}
		}
	}

	TSharedPtr<FJsonValueArray> SerializeArray(const TArray<TSharedPtr<DocTreeNode>> ArrayElements)
	{
		TArray<TSharedPtr<FJsonValue>> OutArray;
		for (auto& Element : ArrayElements)
		{
			TSharedPtr<FJsonValue> NewElementValue;
			Element->SerializeWith(MakeShared<DocGenJsonSerializer>(NewElementValue));
			OutArray.Add(NewElementValue);
		}
		return MakeShared<FJsonValueArray>(OutArray);
	}

	virtual void SerializeString(const FString& InString) override
	{
		TargetObject = MakeShared<FJsonValueString>(InString);
	}
	virtual void SerializeNull() override
	{
		TargetObject = MakeShared<FJsonValueNull>();
	}

public:
	DocGenJsonSerializer(TSharedPtr<FJsonValue>& TargetObject) : TargetObject(TargetObject) {};
	DocGenJsonSerializer()
		: TopLevelObject(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>())),
		  TargetObject(TopLevelObject)
	{}
	virtual bool SaveToFile(const FString& OutFile)
	{
		if (!TopLevelObject)
		{
			return false;
		}
		else
		{
			FString Result;
			auto JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Result);
			FJsonSerializer::Serialize(TopLevelObject->AsObject().ToSharedRef(), JsonWriter);

			return FFileHelper::SaveStringToFile(Result, *OutFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
	};
};

UCLASS()
class UDocGenJsonSerializerFactory : public UObject, public IDocGenSerializerFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<struct DocTreeNode::IDocTreeSerializer> CreateSerializer() override
	{
		return MakeShared<DocGenJsonSerializer>();
	}
};