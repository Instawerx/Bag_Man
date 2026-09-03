// Copyright C12 AI Gaming. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Components/ComboBoxString.h"    // ESelectInfo + the recipient picker
#include "Social/AFLSocialSubsystem.h"    // FAFLDirectMessage (AFLOnline)

#include "AFLW_DirectMessages.generated.h"

class UScrollBox;
class UVerticalBox;
class UEditableTextBox;
class UTextBlock;
class UComboBoxString;

/**
 * UAFLW_DirectMessages -- the COMMS-5 live DM surface (R5 Session/Inbox), pure-C++ like UAFLW_Chat, styled
 * from the ratified IRONICS tokens. It binds UAFLSocialSubsystem (the WebSocket/REST DM transport):
 * FetchInbox on open, OnDirectMessage for live inbound, SendDirectMessage on reply, MarkConversationRead on
 * opening a thread. Recipients + names come from the replicated PlayerState identity -- any in-session
 * player's PlayFabId is UAFLPlayerIdentityComponent::GetResolvedPlayFabId() (DOREPLIFETIME to all clients)
 * and their name is GetPlayerName(), so a player picks a name and never types a raw id.
 *
 * Opened via `afl.DM.Open`. The COMMS-2 chat panel's DM stub points here.
 */
UCLASS()
class AFLCOMBAT_API UAFLW_DirectMessages : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UAFLW_DirectMessages();

	static UAFLW_DirectMessages* Open(const UObject* WorldContext);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

private:
	/** Populate the recipient picker: in-session players (by name) + any inbox conversation partners. */
	void RebuildRecipientList();
	/** Open a conversation with a peer (their PlayFabId), render the thread, mark it read. */
	void SelectConversation(const FString& OtherId, const FString& OtherName);
	/** Repaint the thread from the cache for the current conversation. */
	void RenderThread();
	/** Live inbound (and the sender echo) from the transport. */
	void HandleInbound(const FAFLDirectMessage& Msg);

	UFUNCTION()
	void OnRecipientSelected(FString SelectedItem, ESelectInfo::Type SelectType);

	UFUNCTION()
	void OnReplyCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** This client's own verified PlayFabId (local PlayerState identity component). */
	FString MyPlayFabId() const;
	/** PlayFabId -> display name via the in-session PlayerStates; falls back to a short id tag. */
	FString ResolveName(const FString& PlayFabId) const;
	/** conversationId = sorted pair 'a#b' (matches the backend). */
	static FString ConversationOf(const FString& A, const FString& B);

	UPROPERTY(Transient) TObjectPtr<UComboBoxString>  RecipientCombo;
	UPROPERTY(Transient) TObjectPtr<UScrollBox>       ThreadScroll;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox>     ThreadBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ReplyInput;
	UPROPERTY(Transient) TObjectPtr<UTextBlock>       RecipientLabel;

	/** Combo display string -> that person's PlayFabId (covers in-session players + inbox partners). */
	TMap<FString, FString> DisplayToId;

	FString CurrentOtherId;
	FString CurrentOtherName;
	FString CurrentConversationId;
	TArray<FAFLDirectMessage> Cache; // all known messages this session
	FDelegateHandle DmHandle;
};
