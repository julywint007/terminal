// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TasksPaneContent.h"
#include "PaneArgs.h"
#include "TasksPaneContent.g.cpp"
#include "TaskViewModel.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt
{
    namespace WUX = Windows::UI::Xaml;
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    TasksPaneContent::TasksPaneContent()
    {
        InitializeComponent();

        // _root = this; // winrt::Windows::UI::Xaml::Controls::Grid{};
        // Vertical and HorizontalAlignment are Stretch by default

        auto res = Windows::UI::Xaml::Application::Current().Resources();
        auto bg = res.Lookup(winrt::box_value(L"UnfocusedBorderBrush"));
        /*_root.*/ Background(bg.try_as<WUX::Media::Brush>());

        // _treeView = winrt::Microsoft::UI::Xaml::Controls::TreeView{};
        // _root.Children().Append(_treeView);
        // _box.Margin({ 10, 10, 10, 10 });
        // _box.AcceptsReturn(true);
        // _box.TextWrapping(TextWrapping::Wrap);

        // UpdateSettings(settings);
    }

    MUX::Controls::TreeViewNode _buildTreeViewNode(const Model::Command& task)
    {
        MUX::Controls::TreeViewNode item{};
        item.Content(winrt::box_value(task.Name()));
        if (task.HasNestedCommands())
        {
            for (const auto& [name, nested] : task.NestedCommands())
            {
                item.Children().Append(_buildTreeViewNode(nested));
            }
        }
        return item;
    }

    void TasksPaneContent::UpdateSettings(const CascadiaSettings& settings)
    {
        // _treeView().RootNodes().Clear();
        const auto tasks = settings.GlobalSettings().ActionMap().FilterToSendInput(L""); // IVector<Model::Command>
        // for (const auto& t : tasks)
        // {
        //     const auto& treeNode = _buildTreeViewNode(t);
        //     _treeView().RootNodes().Append(treeNode);
        // }

        auto itemSource = winrt::single_threaded_observable_vector<TerminalApp::TaskViewModel>();
        for (const auto& t : tasks)
        {
            // const auto& treeNode = _buildTreeViewNode(t);
            itemSource.Append(winrt::make<TaskViewModel>(t));
        }
        _treeView().ItemsSource(itemSource);
    }

    winrt::Windows::UI::Xaml::FrameworkElement TasksPaneContent::GetRoot()
    {
        return *this;
    }
    winrt::Windows::Foundation::Size TasksPaneContent::MinSize()
    {
        return { 1, 1 };
    }
    void TasksPaneContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        reason;
        // _box.Focus(reason);
    }
    void TasksPaneContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    NewTerminalArgs TasksPaneContent::GetNewTerminalArgs(const bool /* asContent */) const
    {
        return nullptr;
    }

    winrt::hstring TasksPaneContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

    winrt::Windows::UI::Xaml::Media::Brush TasksPaneContent::BackgroundBrush()
    {
        return Background();
    }

    void TasksPaneContent::SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control)
    {
        _control = control;
    }

    void TasksPaneContent::_runCommandButtonClicked(const Windows::Foundation::IInspectable& sender,
                                                   const Windows::UI::Xaml::RoutedEventArgs&)
    {

        if (const auto& taskVM{ sender.try_as<WUX::Controls::Button>().DataContext().try_as<TaskViewModel>() })
        {
            if (const auto& strongControl{ _control.get() })
            {
                // By using the last active control as the sender here, the
                // actiopn dispatch will send this to the active control,
                // thinking that it is the control that requested this event.
                DispatchCommandRequested.raise(strongControl, taskVM->Command());
            }
        }
    }

}