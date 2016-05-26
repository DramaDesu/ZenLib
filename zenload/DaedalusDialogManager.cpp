#include <utils/logger.h>
#include <algorithm>
#include "DaedalusDialogManager.h"
#include "DaedalusVM.h"
#include "DaedalusStdlib.h"

using namespace GameState;

DaedalusDialogManager::DaedalusDialogManager(ZenLoad::DaedalusVM& vm,const std::string& ou_bin)
    : m_VM(vm),
      m_MessageLib(ou_bin)
{
    gatherNpcInformation();
}

void DaedalusDialogManager::registerExternals(
        std::function<void(NpcHandle, NpcHandle, const ZenLoad::oCMsgConversationData&)> onAIOutput,
        std::function<void(NpcHandle, std::vector<InfoHandle>)> onStartConversation)
{
    m_OnAIOutput = onAIOutput;

    m_VM.registerExternalFunction("ai_output", [&](ZenLoad::DaedalusVM& vm){
        std::string outputname = vm.popString();
        uint32_t target = vm.popVar();
        uint32_t self = vm.popVar();

        auto& message = m_MessageLib.getMessageByName(outputname);

        NpcHandle hself = Memory::handleCast<NpcHandle>(m_VM.getDATFile().getSymbolByIndex(self).instanceDataHandle);
        NpcHandle htarget = Memory::handleCast<NpcHandle>(m_VM.getDATFile().getSymbolByIndex(target).instanceDataHandle);

        // Notify user
        m_OnAIOutput(hself, htarget, message);
    });

    m_VM.registerExternalFunction("AI_ProcessInfos", [&](ZenLoad::DaedalusVM& vm){
        uint32_t self = vm.popVar();

        NpcHandle hself = Memory::handleCast<NpcHandle>(m_VM.getDATFile().getSymbolByIndex(self).instanceDataHandle);
        ZenLoad::GEngineClasses::C_Npc& npc = m_VM.getGameState().getNpc(hself);

        // Notify user
        onStartConversation(hself, m_NpcInfosByNpcSymbols[npc.instanceSymbol]);
    });

    m_VM.registerExternalFunction("InfoManager_HasFinished", [](ZenLoad::DaedalusVM& vm){

        // TODO: Implement this
        vm.setReturn(1);
    });

    m_VM.registerExternalFunction("npc_knowsinfo", [&](ZenLoad::DaedalusVM& vm){
        int32_t infoinstance = vm.popDataValue();
        int32_t self = vm.popVar();

        NpcHandle hself = Memory::handleCast<NpcHandle>(m_VM.getDATFile().getSymbolByIndex(self).instanceDataHandle);
        ZenLoad::GEngineClasses::C_Npc& npc = m_VM.getGameState().getNpc(hself);

        auto& l = m_KnownNpcInfoSymbolsByNpcSymbols[npc.instanceSymbol];
        int32_t knows = l.find(infoinstance) != l.end() ? 1 : 0;

        //LogInfo() << "Does he kow? (" << vm.getDATFile().getSymbolByIndex(npc.instanceSymbol).name << " -> " << vm.getDATFile().getSymbolByIndex(infoinstance).name << "): " << knows;

        vm.setReturn(knows);
    });
}

void DaedalusDialogManager::gatherNpcInformation()
{
    m_VM.getDATFile().iterateSymbolsOfClass("C_Info", [&](size_t i, ZenLoad::PARSymbol& s){

        // Create new info-object
        InfoHandle h = m_VM.getGameState().createInfo();
        ZenLoad::GEngineClasses::C_Info& info = m_VM.getGameState().getInfo(h);
        m_VM.initializeInstance(Memory::toBigHandle(h), i, ZenLoad::IC_Info);

        // Add to map
        m_NpcInfosByNpcSymbols[info.npc].push_back(h);
    });

    // Messages are in wrong order. Fix this.
    for(auto& v : m_NpcInfosByNpcSymbols)
    {
        std::reverse(v.second.begin(),v.second.end());
    }
}

void DaedalusDialogManager::setNpcInfoKnown(size_t npcInstance, size_t infoInstance)
{
    //LogInfo() << "He knows! (" << m_VM.getDATFile().getSymbolByIndex(npcInstance).name << " -> " << m_VM.getDATFile().getSymbolByIndex(infoInstance).name << ")!";
    m_KnownNpcInfoSymbolsByNpcSymbols[npcInstance].insert(infoInstance);
}







