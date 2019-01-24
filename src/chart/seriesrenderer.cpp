/**
 *  OSM
 *  Copyright (C) 2018  Pavel Smokotnin

 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "seriesrenderer.h"

#include <QQuickWindow>

#include "seriesfbo.h"
#include "plot.h"

using namespace Fftchart;

SeriesRenderer::SeriesRenderer() :
    QQuickFramebufferObject::Renderer(),
    m_item(nullptr),
    m_program()
{
}
SeriesRenderer::~SeriesRenderer()
{
}
QOpenGLFramebufferObject *SeriesRenderer::createFramebufferObject(const QSize &size)
{
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);

    openGLFunctions = QOpenGLContext::currentContext()->functions();
    return new QOpenGLFramebufferObject(size, format);
}
void SeriesRenderer::synchronize(QQuickFramebufferObject *item)
{
    m_item = item;
    m_source = static_cast<SeriesFBO*>(item)->source();
}
void SeriesRenderer::render()
{
    if (!m_item)
        return;

    Plot *plot = static_cast<Plot*>(m_item->parent());
    std::lock_guard<std::mutex> guard(plot->renderMutex);

    //plot could be destroyed (or prepared for it) while mutex was locked
    if (!plot || !plot->parent()) {
        return;
    }

    //The item does not have a window until it has been assigned into a scene.
    if (!m_item->window())
        return;

    if (!m_program.isLinked()) {
        qDebug() << "shader not setted or linked";
        return;
    }

    qreal retinaScale = m_item->window()->devicePixelRatio();
    m_width  = static_cast<GLsizei>(m_item->width() * retinaScale);
    m_height = static_cast<GLsizei>(m_item->height() * retinaScale);
    openGLFunctions->glViewport(
        0,
        0,
        m_width,
        m_height
    );

    openGLFunctions->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    openGLFunctions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


    m_program.bind();

    m_program.setUniformValue(
        m_colorUniform,
        static_cast<GLfloat>(m_source->color().redF()),
        static_cast<GLfloat>(m_source->color().greenF()),
        static_cast<GLfloat>(m_source->color().blueF()),
        static_cast<GLfloat>(m_source->color().alphaF())
    );

    m_source->lock();
    renderSeries();
    m_source->unlock();

    m_program.release();
}