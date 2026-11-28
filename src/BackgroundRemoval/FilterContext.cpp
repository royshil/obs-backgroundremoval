// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "FilterContext.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

#include <obs-module.h>

#include <BackgroundRemoval/IRenderingPipeline.hpp>
#include <PluginGlobal/PluginLoader.h>
#include <UI/AboutDialogIntegration.hpp>
#include <plugin-support.h>

#include "PipelineFactory.hpp"

namespace
{

void appendHtmlEscaped(std::string &output, std::string_view text)
{
    for (const char character : text)
    {
        switch (character)
        {
        case '&':
            output += "&amp;";
            break;
        case '<':
            output += "&lt;";
            break;
        case '>':
            output += "&gt;";
            break;
        case '"':
            output += "&quot;";
            break;
        default:
            output += character;
            break;
        }
    }
}

} // namespace

namespace BackgroundRemoval
{

FilterContext::FilterContext(obs_data_t *settings, obs_source_t *source) : source_{source}
{
    if (!settings || !source_)
    {
        throw std::invalid_argument("Invalid background-removal context arguments");
    }
    readSettings(settings);
}

FilterContext::~FilterContext() noexcept
{
    obs_enter_graphics();
    pipeline_.reset();
    obs_leave_graphics();
}

const char *FilterContext::getName(void *) noexcept
{
    return obs_module_text("Background Removal");
}

void *FilterContext::create(obs_data_t *settings, obs_source_t *source) noexcept
{
    try
    {
        return new FilterContext(settings, source);
    }
    catch (const std::exception &exception)
    {
        obs_log(LOG_ERROR, "Failed to create FilterContext: %s", exception.what());
        return nullptr;
    }
    catch (...)
    {
        obs_log(LOG_ERROR, "Failed to create FilterContext: Unknown error");
        return nullptr;
    }
}

void FilterContext::destroy(void *data) noexcept
{
    delete static_cast<FilterContext *>(data);
}

auto FilterContext::handleAboutButtonClicked(obs_properties_t *, obs_property_t *, void *) -> bool
{
    static_cast<void>(AboutDialogIntegration::show());
    return false;
}

void FilterContext::update(void *data, obs_data_t *settings) noexcept
{
    if (auto *context = static_cast<FilterContext *>(data))
    {
        context->readSettings(settings);
    }
}

void FilterContext::readSettings(obs_data_t *settings) noexcept
{
    pipelineId_.store(obs_data_get_int(settings, "v2_PipelineId"));
    blurBgEnabled_.store(obs_data_get_bool(settings, "v2_BlurBg"));
    blurBgStrength_.store(obs_data_get_double(settings, "v2_BlurBg_Factor"));
    blurBgFocusRatio_.store(obs_data_get_double(settings, "v2_BlurBg_FocusRatio"));
}

void FilterContext::applySettings(IRenderingPipeline &pipeline) const noexcept
{
    pipeline.setBlurBgEnabled(blurBgEnabled_.load());
    pipeline.setBlurBgStrength(blurBgStrength_.load());
    pipeline.setBlurBgFocusRatio(blurBgFocusRatio_.load());
}

void FilterContext::getDefaults(obs_data_t *settings) noexcept
{
    obs_data_set_default_int(settings, "v2_PipelineId", kAutoPipelineId);
    obs_data_set_default_bool(settings, "v2_BlurBg", false);
    obs_data_set_default_double(settings, "v2_BlurBg_Factor", 4.0);
    obs_data_set_default_int(settings, "v2_BlurBg_FocusRatio", -100);
}

std::string FilterContext::getVersionNotice()
{
    std::string versionNotice = "<a href=\"https://github.com/royshil/obs-backgroundremoval/releases\">";
    appendHtmlEscaped(versionNotice, obs_module_text("Background Removal"));
    versionNotice += " ";
    appendHtmlEscaped(versionNotice, PLUGIN_VERSION);
    // The remote latest version stays 0.0.0 until it has been fetched.
    const pluginloader_semver latestVersion = pluginloader_get_remote_latest_version();
    if (std::ranges::any_of(latestVersion.version_core, [](long component) { return component != 0; }))
    {
        PluginLoaderSemver currentVersion{};
        const bool currentVersionParsed = currentVersion.parse(PLUGIN_VERSION);

        versionNotice += " (";
        if (currentVersionParsed &&
            std::ranges::lexicographical_compare(currentVersion.version_core, latestVersion.version_core))
        {
            appendHtmlEscaped(versionNotice, obs_module_text("New Version Available!"));
        }
        else
        {
            appendHtmlEscaped(versionNotice, obs_module_text("Up to Date"));
        }
        versionNotice += ")";
    }
    versionNotice += "</a>";
    return versionNotice;
}

obs_properties_t *FilterContext::getProperties(void *data) noexcept
{
    obs_properties_t *const p = obs_properties_create();

    // MARK: - Header

    try
    {
        std::string versionNotice = getVersionNotice();
        obs_properties_add_text(p, "v2_VersionNotice", versionNotice.c_str(), OBS_TEXT_INFO);
    }
    catch (const std::exception &exception)
    {
        obs_log(LOG_ERROR, "Failed to get version info: %s", exception.what());
        obs_properties_add_text(p, "v2_VersionNotice", obs_module_text("Background Removal"), OBS_TEXT_INFO);
    }
    catch (...)
    {
        obs_log(LOG_ERROR, "Failed to get version info: Unknown error");
        obs_properties_add_text(p, "v2_VersionNotice", obs_module_text("Background Removal"), OBS_TEXT_INFO);
    }

    // MARK: - General Properties

    obs_properties_t *const pGeneral = obs_properties_create();
    obs_properties_add_group(p, "v2_GeneralGroup", obs_module_text("General"), OBS_GROUP_NORMAL, pGeneral);

    obs_property_t *const pipeline = obs_properties_add_list(pGeneral, "v2_PipelineId", obs_module_text("Pipeline"),
                                                             OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(pipeline, obs_module_text("Auto"), kAutoPipelineId);
    obs_property_list_add_int(pipeline, obs_module_text("MediaPipe (Landscape) - Default GPU"), kMPLSPipelineId);
    obs_property_set_long_description(
        pipeline,
        obs_module_text("Auto selects the recommended pipeline. Select a specific pipeline to use it explicitly."));

    // MARK: - Blur Background Properties

    obs_properties_t *const blurProperties = obs_properties_create();
    obs_properties_add_group(p, "v2_BlurBg", obs_module_text("Blur Background"), OBS_GROUP_CHECKABLE, blurProperties);

    obs_property_t *const blurStrength = obs_properties_add_float_slider(blurProperties, "v2_BlurBg_Factor",
                                                                         obs_module_text("Strength"), 0.0, 20.0, 0.1);
    obs_property_set_long_description(
        blurStrength,
        obs_module_text(
            "How strongly the parts farthest from the focus point are blurred. 0 leaves the image sharp. The default is 4."));
    obs_property_t *const blurFocusRatio = obs_properties_add_int_slider(blurProperties, "v2_BlurBg_FocusRatio",
                                                                         obs_module_text("Focus Ratio"), -100, 100, 1);
    obs_property_int_set_suffix(blurFocusRatio, "%");
    obs_property_set_long_description(
        blurFocusRatio,
        obs_module_text(
            "Where the image stays sharp: -100% keeps the person sharp, 100% keeps the background sharp, and 0% balances the two. The blur grows with the distance from there. The default is -100%."));

    // MARK: - Footer

    obs_properties_add_button2(p, "v2_AboutButton", obs_module_text("About this plugin"), handleAboutButtonClicked,
                               data);

    return p;
}

// The output size is the one of the current pipeline, as video_tick created it, or 0 without one.
// These callbacks query no source size: for the parent source, obs_source_get_width/height() return
// the size of its first filter, which may be this filter, under the parent's filter mutex.
std::uint32_t FilterContext::getWidth(void *data) noexcept
{
    const auto *context = static_cast<FilterContext *>(data);
    return context ? context->outputWidth_.load() : 0;
}

std::uint32_t FilterContext::getHeight(void *data) noexcept
{
    const auto *context = static_cast<FilterContext *>(data);
    return context ? context->outputHeight_.load() : 0;
}

void FilterContext::videoTick(void *data, float) noexcept
{
    if (auto *context = static_cast<FilterContext *>(data))
    {
        context->tick();
    }
}

// Recreates the pipeline whenever the selection or the target's size changes, as
// live-backgroundremoval-lite does, with the target's base size. It works while the parent source is
// showing anywhere, not only while it is active in the program output, so that the Studio Mode
// preview, the filter properties preview and projectors show the source with the background
// removed. A key that failed is not retried until something changes again. video_tick runs on the
// graphics thread outside the graphics context, so enter it to create or destroy the pipeline.
void FilterContext::tick() noexcept
{
    obs_source_t *const parent = obs_filter_get_parent(source_);
    obs_source_t *const target = obs_filter_get_target(source_);
    if (!parent || !obs_source_showing(parent) || !target)
    {
        return;
    }
    const PipelineKey key{
        .pipelineId = pipelineId_.load(),
        .sourceWidth = obs_source_get_base_width(target),
        .sourceHeight = obs_source_get_base_height(target),
    };
    if (key.sourceWidth == 0 || key.sourceHeight == 0)
    {
        resetPipeline();
        return;
    }
    if (pipelineKey_ == key)
    {
        return;
    }
    pipelineKey_ = key;

    obs_enter_graphics();
    pipeline_.reset();
    try
    {
        pipeline_ = createRenderingPipeline(key.pipelineId, source_, key.sourceWidth, key.sourceHeight);
    }
    catch (const std::exception &exception)
    {
        obs_log(LOG_ERROR, "Failed to create the rendering pipeline: %s", exception.what());
    }
    catch (...)
    {
        obs_log(LOG_ERROR, "Failed to create the rendering pipeline: Unknown error");
    }
    obs_leave_graphics();
    outputWidth_.store(pipeline_ ? pipeline_->width() : 0);
    outputHeight_.store(pipeline_ ? pipeline_->height() : 0);
}

void FilterContext::resetPipeline() noexcept
{
    if (pipeline_)
    {
        obs_enter_graphics();
        pipeline_.reset();
        obs_leave_graphics();
    }
    pipelineKey_.reset();
    outputWidth_.store(0);
    outputHeight_.store(0);
}

void FilterContext::videoRender(void *data, gs_effect_t *) noexcept
{
    if (auto *context = static_cast<FilterContext *>(data))
    {
        context->render();
    }
}

// Draws nothing when the parent is not shown or there is no pipeline, as live-backgroundremoval-lite
// does, so that the unfiltered background is never disclosed.
void FilterContext::render() noexcept
{
    obs_source_t *const parent = obs_filter_get_parent(source_);
    if (!parent || !obs_source_showing(parent) || !pipeline_)
    {
        return;
    }
    applySettings(*pipeline_);
    pipeline_->videoRender();
}

} // namespace BackgroundRemoval
