﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "Widgets/Anim/UEFAnimImportOptions.h"
#include "UEFAnimFactory.generated.h"


UCLASS(hidecategories = Object)
class UEFORMAT_API UEFAnimFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UEFAnimImportOptions* SettingsImporter;
	bool bImport;
	bool bImportAll;

	UPROPERTY(BlueprintReadWrite)
	bool bSilentImport;


	virtual UObject* FactoryCreateFile(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, const FString& Filename, const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
};