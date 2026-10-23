#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RBPlayerController.generated.h"

UCLASS()
class REPLICATIONBUDGETER_API ARBPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void SetupInputComponent() override;
    bool IsDebugEnabled() const { return bDebugEnabled; }

    UFUNCTION(Exec)
    void RBExportCSV();

    UFUNCTION(Exec)
    void RBClearSamples();

    UFUNCTION(Exec)
    void RBSetMode(int32 Optimized);

    UFUNCTION(Exec)
    void RBSetCount(int32 Count);

private:
    void TogglePolicy();
    void ToggleVisuals();
    void Choose50();
    void Choose200();
    void Choose500();

    UFUNCTION(Server, Reliable)
    void ServerSetMode(bool bEnabled);

    UFUNCTION(Server, Reliable)
    void ServerSetCount(int32 Count);

    bool bDebugEnabled = true;
};
