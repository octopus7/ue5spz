#include "SpzAssetFactory.h"

#include "EditorFramework/AssetImportData.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SpzAsset.h"
#include "SpzEditor.h"
#include "load-spz.h"

#include <cmath>
#include <vector>

#define LOCTEXT_NAMESPACE "SpzAssetFactory"

namespace
{
constexpr float Sh0ToRgbScale = 0.28209479177387814f;

float Sigmoid(float Value)
{
	return 1.0f / (1.0f + FMath::Exp(-Value));
}

int32 ShCoefficientCountForDegree(int32 Degree)
{
	switch (Degree)
	{
	case 0:
		return 0;
	case 1:
		return 9;
	case 2:
		return 24;
	case 3:
		return 45;
	case 4:
		return 72;
	default:
		return 0;
	}
}

FVector ConvertPosition(const float* Source, float ImportScale, bool bConvertCoordinatesToUnreal)
{
	if (bConvertCoordinatesToUnreal)
	{
		return FVector(-Source[2], Source[0], Source[1]) * ImportScale;
	}

	return FVector(Source[0], Source[1], Source[2]) * ImportScale;
}

FVector ConvertScale(const float* SourceLogScale, float ImportScale, bool bConvertCoordinatesToUnreal)
{
	const FVector SourceScale(FMath::Exp(SourceLogScale[0]), FMath::Exp(SourceLogScale[1]), FMath::Exp(SourceLogScale[2]));
	if (bConvertCoordinatesToUnreal)
	{
		return FVector(SourceScale.Z, SourceScale.X, SourceScale.Y) * ImportScale;
	}

	return SourceScale * ImportScale;
}

FQuat ConvertRotation(const float* SourceQuaternion)
{
	return FQuat(SourceQuaternion[0], SourceQuaternion[1], SourceQuaternion[2], SourceQuaternion[3]).GetNormalized();
}

FLinearColor ConvertColor(const float* SourceColor)
{
	return FLinearColor(
		FMath::Clamp(0.5f + Sh0ToRgbScale * SourceColor[0], 0.0f, 1.0f),
		FMath::Clamp(0.5f + Sh0ToRgbScale * SourceColor[1], 0.0f, 1.0f),
		FMath::Clamp(0.5f + Sh0ToRgbScale * SourceColor[2], 0.0f, 1.0f),
		1.0f);
}
} // namespace

USpzAssetFactory::USpzAssetFactory()
{
	SupportedClass = USpzAsset::StaticClass();
	bCreateNew = false;
	bEditorImport = true;
	Formats.Add(TEXT("spz;SPZ Gaussian Splat"));
}

bool USpzAssetFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename).Equals(TEXT("spz"), ESearchCase::IgnoreCase);
}

UObject* USpzAssetFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	USpzAsset* Asset = NewObject<USpzAsset>(InParent, InClass, InName, Flags);
	if (!Asset)
	{
		return nullptr;
	}

	if (!ImportIntoAsset(Asset, Filename, Warn))
	{
		return nullptr;
	}

#if WITH_EDITORONLY_DATA
	if (Asset->AssetImportData)
	{
		Asset->AssetImportData->Update(Filename);
	}
#endif

	return Asset;
}

bool USpzAssetFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const USpzAsset* Asset = Cast<USpzAsset>(Obj);
	if (!Asset)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (Asset->AssetImportData)
	{
		Asset->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
#endif

	return false;
}

void USpzAssetFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	USpzAsset* Asset = Cast<USpzAsset>(Obj);
	if (!Asset || NewReimportPaths.IsEmpty())
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	if (Asset->AssetImportData)
	{
		Asset->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
#endif
}

EReimportResult::Type USpzAssetFactory::Reimport(UObject* Obj)
{
	USpzAsset* Asset = Cast<USpzAsset>(Obj);
	if (!Asset)
	{
		return EReimportResult::Failed;
	}

#if WITH_EDITORONLY_DATA
	if (!Asset->AssetImportData)
	{
		return EReimportResult::Failed;
	}

	const FString Filename = Asset->AssetImportData->GetFirstFilename();
	if (Filename.IsEmpty() || !FPaths::FileExists(Filename))
	{
		return EReimportResult::Failed;
	}

	Asset->Modify();
	if (!ImportIntoAsset(Asset, Filename, GWarn))
	{
		return EReimportResult::Failed;
	}

	Asset->AssetImportData->Update(Filename);
	Asset->MarkPackageDirty();
	return EReimportResult::Succeeded;
#else
	return EReimportResult::Failed;
#endif
}

int32 USpzAssetFactory::GetPriority() const
{
	return ImportPriority + 1;
}

bool USpzAssetFactory::ImportIntoAsset(USpzAsset* Asset, const FString& Filename, FFeedbackContext* Warn) const
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *Filename))
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("Failed to read SPZ file: %s"), *Filename);
		}
		return false;
	}

	std::vector<uint8_t> Bytes;
	Bytes.assign(FileData.GetData(), FileData.GetData() + FileData.Num());

	spz::UnpackOptions Options;
	Options.to = spz::CoordinateSystem::RUB;

	const spz::GaussianCloud Cloud = spz::loadSpz(Bytes, Options);
	if (Cloud.numPoints <= 0)
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("SPZ decode produced no splats: %s"), *Filename);
		}
		return false;
	}

	const int32 NumPoints = Cloud.numPoints;
	const int32 ExpectedShValues = NumPoints * ShCoefficientCountForDegree(Cloud.shDegree);
	if (Cloud.positions.size() != static_cast<size_t>(NumPoints * 3) ||
		Cloud.scales.size() != static_cast<size_t>(NumPoints * 3) ||
		Cloud.rotations.size() != static_cast<size_t>(NumPoints * 4) ||
		Cloud.alphas.size() != static_cast<size_t>(NumPoints) ||
		Cloud.colors.size() != static_cast<size_t>(NumPoints * 3) ||
		(Cloud.shDegree > 0 && Cloud.sh.size() != static_cast<size_t>(ExpectedShValues)))
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error, TEXT("SPZ decoded buffers have unexpected sizes: %s"), *Filename);
		}
		return false;
	}

	TArray<FVector3f> Positions;
	TArray<FVector4f> Rotations;
	TArray<FVector3f> Scales;
	TArray<FColor> Colors;
	Positions.Reserve(NumPoints);
	Rotations.Reserve(NumPoints);
	Scales.Reserve(NumPoints);
	Colors.Reserve(NumPoints);
	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		const FVector Position = ConvertPosition(&Cloud.positions[Index * 3], ImportScale, bConvertCoordinatesToUnreal);
		const FVector Scale = ConvertScale(&Cloud.scales[Index * 3], ImportScale, bConvertCoordinatesToUnreal);
		const FQuat Rotation = ConvertRotation(&Cloud.rotations[Index * 4]);
		FLinearColor Color = ConvertColor(&Cloud.colors[Index * 3]);
		const float Opacity = FMath::Clamp(Sigmoid(Cloud.alphas[Index]), 0.0f, 1.0f);
		Color.A = Opacity;

		Positions.Add(FVector3f(Position));
		Rotations.Add(FVector4f(static_cast<float>(Rotation.X), static_cast<float>(Rotation.Y), static_cast<float>(Rotation.Z), static_cast<float>(Rotation.W)));
		Scales.Add(FVector3f(Scale));

		FColor StoredColor = Color.ToFColor(false);
		StoredColor.A = static_cast<uint8>(FMath::RoundToInt(Opacity * 255.0f));
		Colors.Add(StoredColor);
	}

	TArray<float> SphericalHarmonics;
	if (!Cloud.sh.empty())
	{
		SphericalHarmonics.SetNumUninitialized(static_cast<int32>(Cloud.sh.size()));
		FMemory::Memcpy(SphericalHarmonics.GetData(), Cloud.sh.data(), Cloud.sh.size() * sizeof(float));
	}

	Asset->InitializeFromArrays(
		MoveTemp(Positions),
		MoveTemp(Rotations),
		MoveTemp(Scales),
		MoveTemp(Colors),
		MoveTemp(SphericalHarmonics),
		Cloud.shDegree,
		Cloud.antialiased,
		TEXT("SPZ"));
	UE_LOG(LogSpzEditor, Log, TEXT("Imported %d splats from %s"), Asset->NumSplats, *Filename);
	return true;
}

#undef LOCTEXT_NAMESPACE
