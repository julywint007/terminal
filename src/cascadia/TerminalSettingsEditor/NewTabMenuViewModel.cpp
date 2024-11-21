// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NewTabMenuViewModel.h"
#include <LibraryResources.h>

#include "NewTabMenuViewModel.g.cpp"
#include "FolderTreeViewEntry.g.cpp"
#include "NewTabMenuEntryViewModel.g.cpp"
#include "ProfileEntryViewModel.g.cpp"
#include "ActionEntryViewModel.g.cpp"
#include "SeparatorEntryViewModel.g.cpp"
#include "FolderEntryViewModel.g.cpp"
#include "MatchProfilesEntryViewModel.g.cpp"
#include "RemainingProfilesEntryViewModel.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Xaml::Data;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static IObservableVector<Editor::NewTabMenuEntryViewModel> _ConvertToViewModelEntries(const IVector<Model::NewTabMenuEntry>& settingsModelEntries, const Model::CascadiaSettings& settings)
    {
        std::vector<Editor::NewTabMenuEntryViewModel> result{};
        if (!settingsModelEntries)
        {
            return single_threaded_observable_vector<Editor::NewTabMenuEntryViewModel>(std::move(result));
        }

        for (const auto& entry : settingsModelEntries)
        {
            switch (entry.Type())
            {
            case NewTabMenuEntryType::Profile:
            {
                // If the Profile isn't set, this is an invalid entry. Skip it.
                if (const auto& profileEntry = entry.as<Model::ProfileEntry>(); profileEntry.Profile())
                {
                    result.push_back(make<ProfileEntryViewModel>(profileEntry));
                }
                break;
            }
            case NewTabMenuEntryType::Action:
            {
                if (const auto& actionEntry = entry.as<Model::ActionEntry>())
                {
                    result.push_back(make<ActionEntryViewModel>(actionEntry, settings));
                }
                break;
            }
            case NewTabMenuEntryType::Separator:
            {
                if (const auto& separatorEntry = entry.as<Model::SeparatorEntry>())
                {
                    result.push_back(make<SeparatorEntryViewModel>(separatorEntry));
                }
                break;
            }
            case NewTabMenuEntryType::Folder:
            {
                if (const auto& folderEntry = entry.as<Model::FolderEntry>())
                {
                    // The ctor will convert the children of the folder to view models
                    result.push_back(make<FolderEntryViewModel>(folderEntry, settings));
                }
                break;
            }
            case NewTabMenuEntryType::MatchProfiles:
            {
                if (const auto& matchProfilesEntry = entry.as<Model::MatchProfilesEntry>())
                {
                    result.push_back(make<MatchProfilesEntryViewModel>(matchProfilesEntry));
                }
                break;
            }
            case NewTabMenuEntryType::RemainingProfiles:
            {
                if (const auto& remainingProfilesEntry = entry.as<Model::RemainingProfilesEntry>())
                {
                    result.push_back(make<RemainingProfilesEntryViewModel>(remainingProfilesEntry));
                }
                break;
            }
            case NewTabMenuEntryType::Invalid:
            default:
                break;
            }
        }
        return single_threaded_observable_vector<Editor::NewTabMenuEntryViewModel>(std::move(result));
    }

    bool NewTabMenuViewModel::IsRemainingProfilesEntryMissing() const
    {
        return _IsRemainingProfilesEntryMissing(_rootEntries);
    }

    bool NewTabMenuViewModel::_IsRemainingProfilesEntryMissing(const IVector<Editor::NewTabMenuEntryViewModel>& entries)
    {
        for (const auto& entry : entries)
        {
            switch (entry.Type())
            {
            case NewTabMenuEntryType::RemainingProfiles:
            {
                return false;
            }
            case NewTabMenuEntryType::Folder:
            {
                if (!_IsRemainingProfilesEntryMissing(entry.as<Editor::FolderEntryViewModel>().Entries()))
                {
                    return false;
                }
                break;
            }
            default:
                break;
            }
        }
        return true;
    }

    bool NewTabMenuViewModel::IsFolderView() const noexcept
    {
        return CurrentView() != _rootEntries;
    }

    NewTabMenuViewModel::NewTabMenuViewModel(Model::CascadiaSettings settings)
    {
        UpdateSettings(settings);

        // Add a property changed handler to our own property changed event.
        // This propagates changes from the settings model to anybody listening to our
        // unique view model members.
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto viewModelProperty{ args.PropertyName() };
            if (viewModelProperty == L"AvailableProfiles")
            {
                _NotifyChanges(L"SelectedProfile");
            }
            else if (viewModelProperty == L"CurrentFolder")
            {
                if (_CurrentFolder)
                {
                    CurrentFolderName(_CurrentFolder.Name());
                    _CurrentFolder.PropertyChanged({ this, &NewTabMenuViewModel::_FolderPropertyChanged });
                }
                _NotifyChanges(L"IsFolderView", L"CurrentView");
            }
        });
    }

    void NewTabMenuViewModel::_FolderPropertyChanged(const IInspectable& /*sender*/, const Windows::UI::Xaml::Data::PropertyChangedEventArgs& args)
    {
        const auto viewModelProperty{ args.PropertyName() };
        if (viewModelProperty == L"Name")
        {
            // FolderTree needs to be updated when a folder is renamed
            _folderTreeCache = nullptr;
        }
    }

    hstring NewTabMenuViewModel::CurrentFolderName() const
    {
        if (!_CurrentFolder)
        {
            return {};
        }
        return _CurrentFolder.Name();
    }

    void NewTabMenuViewModel::CurrentFolderName(const hstring& value)
    {
        if (_CurrentFolder && _CurrentFolder.Name() != value)
        {
            _CurrentFolder.Name(value);
            _NotifyChanges(L"CurrentFolderName");
        }
    }

    bool NewTabMenuViewModel::CurrentFolderInlining() const
    {
        if (!_CurrentFolder)
        {
            return {};
        }
        return _CurrentFolder.Inlining();
    }

    void NewTabMenuViewModel::CurrentFolderInlining(bool value)
    {
        if (_CurrentFolder && _CurrentFolder.Inlining() != value)
        {
            _CurrentFolder.Inlining(value);
            _NotifyChanges(L"CurrentFolderInlining");
        }
    }

    bool NewTabMenuViewModel::CurrentFolderAllowEmpty() const
    {
        if (!_CurrentFolder)
        {
            return {};
        }
        return _CurrentFolder.AllowEmpty();
    }

    void NewTabMenuViewModel::CurrentFolderAllowEmpty(bool value)
    {
        if (_CurrentFolder && _CurrentFolder.AllowEmpty() != value)
        {
            _CurrentFolder.AllowEmpty(value);
            _NotifyChanges(L"CurrentFolderAllowEmpty");
        }
    }

    Windows::Foundation::Collections::IObservableVector<Editor::NewTabMenuEntryViewModel> NewTabMenuViewModel::CurrentView() const
    {
        if (!_CurrentFolder)
        {
            return _rootEntries;
        }
        return _CurrentFolder.Entries();
    }

    void NewTabMenuViewModel::UpdateSettings(const Model::CascadiaSettings& settings)
    {
        _Settings = settings;
        _NotifyChanges(L"AvailableProfiles");

        SelectedProfile(AvailableProfiles().GetAt(0));

        _rootEntries = _ConvertToViewModelEntries(_Settings.GlobalSettings().NewTabMenu(), _Settings);

        _rootEntriesChangedRevoker = _rootEntries.VectorChanged(winrt::auto_revoke, [this](auto&&, const IVectorChangedEventArgs& args) {
            switch (args.CollectionChange())
            {
            case CollectionChange::Reset:
            {
                // fully replace settings model with view model structure
                std::vector<Model::NewTabMenuEntry> modelEntries;
                for (const auto& entry : _rootEntries)
                {
                    modelEntries.push_back(NewTabMenuEntryViewModel::GetModel(entry));
                }
                _Settings.GlobalSettings().NewTabMenu(single_threaded_vector<Model::NewTabMenuEntry>(std::move(modelEntries)));
                return;
            }
            case CollectionChange::ItemInserted:
            {
                const auto& insertedEntryVM = _rootEntries.GetAt(args.Index());
                const auto& insertedEntry = NewTabMenuEntryViewModel::GetModel(insertedEntryVM);
                _Settings.GlobalSettings().NewTabMenu().InsertAt(args.Index(), insertedEntry);
                return;
            }
            case CollectionChange::ItemRemoved:
            {
                _Settings.GlobalSettings().NewTabMenu().RemoveAt(args.Index());
                return;
            }
            case CollectionChange::ItemChanged:
            {
                const auto& modifiedEntry = _rootEntries.GetAt(args.Index());
                _Settings.GlobalSettings().NewTabMenu().SetAt(args.Index(), NewTabMenuEntryViewModel::GetModel(modifiedEntry));
                return;
            }
            }
        });

        // Clear CurrentFolder to reset the view
        _CurrentFolder = nullptr;
    }

    void NewTabMenuViewModel::RequestReorderEntry(const Editor::NewTabMenuEntryViewModel& vm, bool goingUp)
    {
        uint32_t idx;
        if (CurrentView().IndexOf(vm, idx))
        {
            if (goingUp && idx > 0)
            {
                CurrentView().RemoveAt(idx);
                CurrentView().InsertAt(idx - 1, vm);
            }
            else if (!goingUp && idx < CurrentView().Size() - 1)
            {
                CurrentView().RemoveAt(idx);
                CurrentView().InsertAt(idx + 1, vm);
            }
        }
    }

    void NewTabMenuViewModel::RequestDeleteEntry(const Editor::NewTabMenuEntryViewModel& vm)
    {
        uint32_t idx;
        if (CurrentView().IndexOf(vm, idx))
        {
            CurrentView().RemoveAt(idx);

            if (vm.try_as<Editor::FolderEntryViewModel>())
            {
                _folderTreeCache = nullptr;
            }
        }
    }

    void NewTabMenuViewModel::RequestMoveEntriesToFolder(const Windows::Foundation::Collections::IVector<Editor::NewTabMenuEntryViewModel>& entries, const Editor::FolderEntryViewModel& destinationFolder)
    {
        auto destination{ destinationFolder == nullptr ? _rootEntries : destinationFolder.Entries() };
        for (auto&& e : entries)
        {
            // Don't move the folder into itself (just skip over it)
            if (e == destinationFolder)
            {
                continue;
            }

            // Remove entry from the current layer,
            // and add it to the destination folder
            RequestDeleteEntry(e);
            destination.Append(e);
        }
    }

    Editor::NewTabMenuEntryViewModel NewTabMenuViewModel::RequestAddSelectedProfileEntry()
    {
        if (_SelectedProfile)
        {
            Model::ProfileEntry profileEntry;
            profileEntry.Profile(_SelectedProfile);

            const auto& entryVM = make<ProfileEntryViewModel>(profileEntry);
            CurrentView().Append(entryVM);
            _PrintAll();
            return entryVM;
        }
        return nullptr;
    }

    Editor::NewTabMenuEntryViewModel NewTabMenuViewModel::RequestAddSeparatorEntry()
    {
        Model::SeparatorEntry separatorEntry;
        const auto& entryVM = make<SeparatorEntryViewModel>(separatorEntry);
        CurrentView().Append(entryVM);

        _PrintAll();
        return entryVM;
    }

    Editor::NewTabMenuEntryViewModel NewTabMenuViewModel::RequestAddFolderEntry()
    {
        Model::FolderEntry folderEntry;
        folderEntry.Name(_AddFolderName);

        const auto& entryVM = make<FolderEntryViewModel>(folderEntry, _Settings);
        CurrentView().Append(entryVM);

        // Reset state after adding the entry
        AddFolderName({});
        _folderTreeCache = nullptr;

        _PrintAll();
        return entryVM;
    }

    Editor::NewTabMenuEntryViewModel NewTabMenuViewModel::RequestAddProfileMatcherEntry()
    {
        Model::MatchProfilesEntry matchProfilesEntry;
        matchProfilesEntry.Name(_ProfileMatcherName);
        matchProfilesEntry.Source(_ProfileMatcherSource);
        matchProfilesEntry.Commandline(_ProfileMatcherCommandline);

        const auto& entryVM = make<MatchProfilesEntryViewModel>(matchProfilesEntry);
        CurrentView().Append(entryVM);

        // Clear the fields after adding the entry
        ProfileMatcherName({});
        ProfileMatcherSource({});
        ProfileMatcherCommandline({});

        _PrintAll();
        return entryVM;
    }

    Editor::NewTabMenuEntryViewModel NewTabMenuViewModel::RequestAddRemainingProfilesEntry()
    {
        Model::RemainingProfilesEntry remainingProfilesEntry;
        const auto& entryVM = make<RemainingProfilesEntryViewModel>(remainingProfilesEntry);
        CurrentView().Append(entryVM);

        _NotifyChanges(L"IsRemainingProfilesEntryMissing");

        _PrintAll();
        return entryVM;
    }

    void NewTabMenuViewModel::GenerateFolderTree()
    {
        if (!_folderTreeCache)
        {
            // Add the root folder
            auto root = winrt::make<FolderTreeViewEntry>(nullptr);

            for (const auto&& entry : _rootEntries)
            {
                if (entry.Type() == NewTabMenuEntryType::Folder)
                {
                    root.Children().Append(winrt::make<FolderTreeViewEntry>(entry.as<Editor::FolderEntryViewModel>()));
                }
            }

            std::vector<Editor::FolderTreeViewEntry> folderTreeCache;
            folderTreeCache.emplace_back(std::move(root));
            _folderTreeCache = single_threaded_observable_vector<Editor::FolderTreeViewEntry>(std::move(folderTreeCache));

            _NotifyChanges(L"FolderTree");
        }
    }

    Collections::IObservableVector<Editor::FolderTreeViewEntry> NewTabMenuViewModel::FolderTree() const
    {
        // We could do this...
        //   if (!_folderTreeCache){ GenerateFolderTree(); }
        // But FolderTree() gets called when we open the page.
        // Instead, we generate the tree as needed using GenerateFolderTree()
        //  which caches the tree.
        return _folderTreeCache;
    }

    // This recursively constructs the FolderTree
    FolderTreeViewEntry::FolderTreeViewEntry(Editor::FolderEntryViewModel folderEntry) :
        _folderEntry{ folderEntry },
        _Children{ single_threaded_observable_vector<Editor::FolderTreeViewEntry>() }
    {
        if (!_folderEntry)
        {
            return;
        }

        for (const auto&& entry : _folderEntry.Entries())
        {
            if (entry.Type() == NewTabMenuEntryType::Folder)
            {
                _Children.Append(winrt::make<FolderTreeViewEntry>(entry.as<Editor::FolderEntryViewModel>()));
            }
        }
    }

    hstring FolderTreeViewEntry::Name() const
    {
        if (!_folderEntry)
        {
            return RS_(L"NewTabMenu_RootFolderName");
        }
        return _folderEntry.Name();
    }

    hstring FolderTreeViewEntry::Icon() const
    {
        if (!_folderEntry)
        {
            return {};
        }
        return _folderEntry.Icon();
    }

    void NewTabMenuViewModel::_PrintAll()
    {
#ifdef _DEBUG
        OutputDebugString(L"---Model:---\n");
        _PrintModel(_Settings.GlobalSettings().NewTabMenu());
        OutputDebugString(L"\n");
        OutputDebugString(L"---VM:---\n");
        _PrintVM(_rootEntries);
        OutputDebugString(L"\n");
#endif
    }

#ifdef _DEBUG
    void NewTabMenuViewModel::_PrintModel(Windows::Foundation::Collections::IVector<Model::NewTabMenuEntry> list, std::wstring prefix)
    {
        if (!list)
        {
            return;
        }

        for (auto&& e : list)
        {
            _PrintModel(e, prefix);
        }
    }

    void NewTabMenuViewModel::_PrintModel(const Model::NewTabMenuEntry& e, std::wstring prefix)
    {
        switch (e.Type())
        {
        case NewTabMenuEntryType::Profile:
        {
            const auto& pe = e.as<Model::ProfileEntry>();
            OutputDebugString(fmt::format(L"{}Profile: {}\n", prefix, pe.Profile().Name()).c_str());
            break;
        }
        case NewTabMenuEntryType::Action:
        {
            const auto& actionEntry = e.as<Model::ActionEntry>();
            OutputDebugString(fmt::format(L"{}Action: {}\n", prefix, actionEntry.ActionId()).c_str());
            break;
        }
        case NewTabMenuEntryType::Separator:
        {
            OutputDebugString(fmt::format(L"{}Separator\n", prefix).c_str());
            break;
        }
        case NewTabMenuEntryType::Folder:
        {
            const auto& fe = e.as<Model::FolderEntry>();
            OutputDebugString(fmt::format(L"{}Folder: {}\n", prefix, fe.Name()).c_str());
            _PrintModel(fe.RawEntries(), prefix + L"  ");
            break;
        }
        case NewTabMenuEntryType::MatchProfiles:
        {
            const auto& matchProfilesEntry = e.as<Model::MatchProfilesEntry>();
            OutputDebugString(fmt::format(L"{}MatchProfiles: {}\n", prefix, matchProfilesEntry.Name()).c_str());
            break;
        }
        case NewTabMenuEntryType::RemainingProfiles:
        {
            OutputDebugString(fmt::format(L"{}RemainingProfiles\n", prefix).c_str());
            break;
        }
        default:
            break;
        }
    }

    void NewTabMenuViewModel::_PrintVM(Windows::Foundation::Collections::IVector<Editor::NewTabMenuEntryViewModel> list, std::wstring prefix)
    {
        if (!list)
        {
            return;
        }

        for (auto&& e : list)
        {
            _PrintVM(e, prefix);
        }
    }

    void NewTabMenuViewModel::_PrintVM(const Editor::NewTabMenuEntryViewModel& e, std::wstring prefix)
    {
        switch (e.Type())
        {
        case NewTabMenuEntryType::Profile:
        {
            const auto& pe = e.as<Editor::ProfileEntryViewModel>();
            OutputDebugString(fmt::format(L"{}Profile: {}\n", prefix, pe.ProfileEntry().Profile().Name()).c_str());
            break;
        }
        case NewTabMenuEntryType::Action:
        {
            const auto& actionEntry = e.as<Editor::ActionEntryViewModel>();
            OutputDebugString(fmt::format(L"{}Action: {}\n", prefix, actionEntry.ActionEntry().ActionId()).c_str());
            break;
        }
        case NewTabMenuEntryType::Separator:
        {
            OutputDebugString(fmt::format(L"{}Separator\n", prefix).c_str());
            break;
        }
        case NewTabMenuEntryType::Folder:
        {
            const auto& fe = e.as<Editor::FolderEntryViewModel>();
            OutputDebugString(fmt::format(L"{}Folder: {}\n", prefix, fe.Name()).c_str());
            _PrintVM(fe.Entries(), prefix + L"  ");
            break;
        }
        case NewTabMenuEntryType::MatchProfiles:
        {
            const auto& matchProfilesEntry = e.as<Editor::MatchProfilesEntryViewModel>();
            OutputDebugString(fmt::format(L"{}MatchProfiles: {}\n", prefix, matchProfilesEntry.DisplayText()).c_str());
            break;
        }
        case NewTabMenuEntryType::RemainingProfiles:
        {
            OutputDebugString(fmt::format(L"{}RemainingProfiles\n", prefix).c_str());
            break;
        }
        default:
            break;
        }
    }
#endif

    NewTabMenuEntryViewModel::NewTabMenuEntryViewModel(const NewTabMenuEntryType type) noexcept :
        _Type{ type }
    {
    }

    Model::NewTabMenuEntry NewTabMenuEntryViewModel::GetModel(const Editor::NewTabMenuEntryViewModel& viewModel)
    {
        switch (viewModel.Type())
        {
        case NewTabMenuEntryType::Profile:
        {
            const auto& projVM = viewModel.as<Editor::ProfileEntryViewModel>();
            return get_self<ProfileEntryViewModel>(projVM)->ProfileEntry();
        }
        case NewTabMenuEntryType::Action:
        {
            const auto& projVM = viewModel.as<Editor::ActionEntryViewModel>();
            return get_self<ActionEntryViewModel>(projVM)->ActionEntry();
        }
        case NewTabMenuEntryType::Separator:
        {
            const auto& projVM = viewModel.as<Editor::SeparatorEntryViewModel>();
            return get_self<SeparatorEntryViewModel>(projVM)->SeparatorEntry();
        }
        case NewTabMenuEntryType::Folder:
        {
            const auto& projVM = viewModel.as<Editor::FolderEntryViewModel>();
            return get_self<FolderEntryViewModel>(projVM)->FolderEntry();
        }
        case NewTabMenuEntryType::MatchProfiles:
        {
            const auto& projVM = viewModel.as<Editor::MatchProfilesEntryViewModel>();
            return get_self<MatchProfilesEntryViewModel>(projVM)->MatchProfilesEntry();
        }
        case NewTabMenuEntryType::RemainingProfiles:
        {
            const auto& projVM = viewModel.as<Editor::RemainingProfilesEntryViewModel>();
            return get_self<RemainingProfilesEntryViewModel>(projVM)->RemainingProfilesEntry();
        }
        case NewTabMenuEntryType::Invalid:
        default:
            return nullptr;
        }
    }

    ProfileEntryViewModel::ProfileEntryViewModel(Model::ProfileEntry profileEntry) :
        ProfileEntryViewModelT<ProfileEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::Profile),
        _ProfileEntry{ profileEntry }
    {
    }

    ActionEntryViewModel::ActionEntryViewModel(Model::ActionEntry actionEntry, Model::CascadiaSettings settings) :
        ActionEntryViewModelT<ActionEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::Action),
        _ActionEntry{ actionEntry },
        _Settings{ settings }
    {
    }

    hstring ActionEntryViewModel::DisplayText() const
    {
        assert(_Settings);

        const auto actionID = _ActionEntry.ActionId();
        if (const auto& action = _Settings.ActionMap().GetActionByID(actionID))
        {
            return action.Name();
        }
        return hstring{ fmt::format(L"{}: {}", RS_(L"NewTabMenu_ActionNotFound"), actionID) };
    }

    hstring ActionEntryViewModel::Icon() const
    {
        assert(_Settings);

        const auto actionID = _ActionEntry.ActionId();
        if (const auto& action = _Settings.ActionMap().GetActionByID(actionID))
        {
            return action.IconPath();
        }
        return {};
    }

    SeparatorEntryViewModel::SeparatorEntryViewModel(Model::SeparatorEntry separatorEntry) :
        SeparatorEntryViewModelT<SeparatorEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::Separator),
        _SeparatorEntry{ separatorEntry }
    {
    }

    FolderEntryViewModel::FolderEntryViewModel(Model::FolderEntry folderEntry) :
        FolderEntryViewModel(folderEntry, nullptr) {}

    FolderEntryViewModel::FolderEntryViewModel(Model::FolderEntry folderEntry, Model::CascadiaSettings settings) :
        FolderEntryViewModelT<FolderEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::Folder),
        _FolderEntry{ folderEntry },
        _Settings{ settings }
    {
        _Entries = _ConvertToViewModelEntries(_FolderEntry.RawEntries(), _Settings);

        _entriesChangedRevoker = _Entries.VectorChanged(winrt::auto_revoke, [this](auto&&, const IVectorChangedEventArgs& args) {
            switch (args.CollectionChange())
            {
            case CollectionChange::Reset:
            {
                // fully replace settings model with _Entries
                std::vector<Model::NewTabMenuEntry> modelEntries;
                for (const auto& entry : _Entries)
                {
                    modelEntries.push_back(NewTabMenuEntryViewModel::GetModel(entry));
                }
                _FolderEntry.RawEntries(single_threaded_vector<Model::NewTabMenuEntry>(std::move(modelEntries)));
                return;
            }
            case CollectionChange::ItemInserted:
            {
                const auto& insertedEntryVM = _Entries.GetAt(args.Index());
                const auto& insertedEntry = NewTabMenuEntryViewModel::GetModel(insertedEntryVM);
                if (!_FolderEntry.RawEntries())
                {
                    _FolderEntry.RawEntries(single_threaded_vector<Model::NewTabMenuEntry>());
                }
                _FolderEntry.RawEntries().InsertAt(args.Index(), insertedEntry);
                return;
            }
            case CollectionChange::ItemRemoved:
            {
                _FolderEntry.RawEntries().RemoveAt(args.Index());
                return;
            }
            case CollectionChange::ItemChanged:
            {
                const auto& modifiedEntry = _Entries.GetAt(args.Index());
                _FolderEntry.RawEntries().SetAt(args.Index(), NewTabMenuEntryViewModel::GetModel(modifiedEntry));
                return;
            }
            }
        });
    }

    bool FolderEntryViewModel::Inlining() const
    {
        return _FolderEntry.Inlining() == FolderEntryInlining::Auto;
    }

    void FolderEntryViewModel::Inlining(bool value)
    {
        const auto valueAsEnum = value ? FolderEntryInlining::Auto : FolderEntryInlining::Never;
        if (_FolderEntry.Inlining() != valueAsEnum)
        {
            _FolderEntry.Inlining(valueAsEnum);
            _NotifyChanges(L"Inlining");
        }
    };

    MatchProfilesEntryViewModel::MatchProfilesEntryViewModel(Model::MatchProfilesEntry matchProfilesEntry) :
        MatchProfilesEntryViewModelT<MatchProfilesEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::MatchProfiles),
        _MatchProfilesEntry{ matchProfilesEntry }
    {
    }

    hstring MatchProfilesEntryViewModel::DisplayText() const
    {
        std::wstring displayText;
        if (const auto profileName = _MatchProfilesEntry.Name(); !profileName.empty())
        {
            fmt::format_to(std::back_inserter(displayText), FMT_COMPILE(L"profile: {}, "), profileName);
        }
        if (const auto commandline = _MatchProfilesEntry.Commandline(); !commandline.empty())
        {
            fmt::format_to(std::back_inserter(displayText), FMT_COMPILE(L"commandline: {}, "), commandline);
        }
        if (const auto source = _MatchProfilesEntry.Source(); !source.empty())
        {
            fmt::format_to(std::back_inserter(displayText), FMT_COMPILE(L"source: {}, "), source);
        }

        // Chop off the last ", "
        displayText.resize(displayText.size() - 2);
        return winrt::hstring{ displayText };
    }

    RemainingProfilesEntryViewModel::RemainingProfilesEntryViewModel(Model::RemainingProfilesEntry remainingProfilesEntry) :
        RemainingProfilesEntryViewModelT<RemainingProfilesEntryViewModel, NewTabMenuEntryViewModel>(Model::NewTabMenuEntryType::RemainingProfiles),
        _RemainingProfilesEntry{ remainingProfilesEntry }
    {
    }
}
